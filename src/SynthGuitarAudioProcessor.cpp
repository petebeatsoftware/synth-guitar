#include "SynthGuitarAudioProcessor.h"

#include "RenderedUserInterface.h"

SynthGuitarAudioProcessor::SynthGuitarAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMS", createParameterLayout()),
      players{ std::make_shared<instrument::TriggeredPlayer>(),
               std::make_shared<instrument::TriggeredPlayer>(),
               std::make_shared<instrument::TriggeredPlayer>() },
      detectors{ instrument::TransientDetector(players[0]),
                 instrument::TransientDetector(players[1]),
                  instrument::TransientDetector(players[2]) },
       singlePlayer(std::make_shared<instrument::TriggeredPlayer>()),
       singleDetector(singlePlayer)
{
    for (size_t band = 0; band < numBands; ++band)
    {
        sensitivityParams[band] = parameters.getRawParameterValue(sensitivityParameterIds[band]);
        players[band]->setChannel(static_cast<int>(band) + 1);
    }
    lowToMidParam = parameters.getRawParameterValue(lowToMidParameterId);
    midToHighParam = parameters.getRawParameterValue(midToHighParameterId);
    detectionModeParam = parameters.getRawParameterValue(detectionModeParameterId);
    bandModeParam = parameters.getRawParameterValue(bandModeParameterId);
    singleSensitivityParam = parameters.getRawParameterValue(singleSensitivityParameterId);
    singleLowFrequencyParam = parameters.getRawParameterValue(singleLowFrequencyParameterId);
    singleHighFrequencyParam = parameters.getRawParameterValue(singleHighFrequencyParameterId);
    strumParam = parameters.getRawParameterValue(strumParameterId);
    velocitySensitivityParam = parameters.getRawParameterValue(velocitySensitivityParameterId);
    singlePlayer->setChannel(1);
}

juce::AudioProcessorValueTreeState::ParameterLayout SynthGuitarAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const char* bandNames[numBands] = { "Low", "Mid", "High" };
    for (int band = 0; band < numBands; ++band)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ sensitivityParameterIds[band], 1 },
            juce::String(bandNames[band]) + " Sensitivity",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
            0.5f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ detectionModeParameterId, 1 }, "Detection Mode",
        juce::StringArray{ "Spectral", "Envelope", "Hybrid" }, 2));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ bandModeParameterId, 1 }, "Mode",
        juce::StringArray{ "Single", "Multi" }, 1));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ strumParameterId, 1 }, "Strum",
        juce::StringArray{ "Low", "High" }, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ singleSensitivityParameterId, 1 }, "Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    auto frequencyRange = juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f);
    frequencyRange.setSkewForCentre(1000.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ lowToMidParameterId, 1 }, "Low to Mid", frequencyRange, 200.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ midToHighParameterId, 1 }, "Mid to High", frequencyRange, 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ singleLowFrequencyParameterId, 1 }, "Low Frequency", frequencyRange, 20.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ singleHighFrequencyParameterId, 1 }, "High Frequency", frequencyRange, 20000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ velocitySensitivityParameterId, 1 }, "Velocity Sensitivity",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    return layout;
}

void SynthGuitarAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    clearNoteActivity();
    for (auto& detector : detectors)
        detector.prepare(sampleRate, samplesPerBlock);
    for (auto& player : players)
        player->prepare(sampleRate, samplesPerBlock);
    singleDetector.prepare(sampleRate, samplesPerBlock);
    singlePlayer->prepare(sampleRate, samplesPerBlock);
    lastBandMode = -1;
    lastStrum = -1;
    singleInputBuffer.ensureSize(1024 * 1024);
    midiScratchBuffer.ensureSize(1024 * 1024);
    for (auto& buffer : splitMidiBuffers)
        buffer.ensureSize(1024 * 1024);
    midiSplitter.reset();
}

void SynthGuitarAudioProcessor::releaseResources()
{
    clearNoteActivity();
    for (auto& detector : detectors)
        detector.reset();
    for (auto& player : players)
        player->reset();
    singleDetector.reset();
    singlePlayer->reset();
    lastBandMode = -1;
    lastStrum = -1;
    singleInputBuffer.clear();
    midiScratchBuffer.clear();
    for (auto& buffer : splitMidiBuffers)
        buffer.clear();
    midiSplitter.reset();
}

bool SynthGuitarAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void SynthGuitarAudioProcessor::processBlock(juce::AudioBuffer<float>& audio,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        audio.clear(channel, 0, audio.getNumSamples());

    midiScratchBuffer.clear();
    midiScratchBuffer.swapWith(midiMessages);
    inputNoteActive.store(updateNoteState(inputNotes, midiScratchBuffer));
    const int bandMode = juce::jlimit(0, 1, static_cast<int>(bandModeParam->load()));
    if (lastBandMode >= 0 && bandMode != lastBandMode)
    {
        if (bandMode == 0)
        {
            for (auto& player : players)
                player->releaseNotes(midiMessages);
            for (auto& detector : detectors)
                detector.reset();
        }
        else
        {
            singlePlayer->releaseNotes(midiMessages);
            singleDetector.reset();
        }
    }
    lastBandMode = bandMode;
    const int strum = bandMode == 0 ? (strumParam->load() >= 0.5f ? 1 : 0) : -1;
    singleInputBuffer.clear();
    if (bandMode == 0 && strum != lastStrum)
    {
        singlePlayer->releaseNotes(midiMessages);
        midiSplitter.addHeldNotes(strum, singleInputBuffer, 0);
    }
    lastStrum = strum;
    midiSplitter.processData(midiScratchBuffer, splitMidiBuffers);
    const auto detectionMode = static_cast<instrument::TransientDetector::DetectionMode>(
        juce::jlimit(0, 2, static_cast<int>(detectionModeParam->load())));
    const auto velocitySensitivity = velocitySensitivityParam->load() / 100.0f;
    if (bandMode == 0)
    {
        const auto low = juce::jlimit(20.0f, 19000.0f, singleLowFrequencyParam->load());
        const auto high = juce::jmin(20000.0f, juce::jmax(singleHighFrequencyParam->load(), low * 1.01f));
        singleDetector.setFrequencyRange(low, high);
        singleInputBuffer.addEvents(splitMidiBuffers[static_cast<size_t>(strum)], 0, -1, 0);
        singlePlayer->processBlock(singleInputBuffer, midiMessages, audio.getNumSamples());
        singleDetector.setSensitivity(singleSensitivityParam->load());
        singleDetector.setVelocitySensitivity(velocitySensitivity);
        singleDetector.setDetectionMode(detectionMode);
        singleDetector.process(audio);
        singlePlayer->finishBlock();
    }
    else
    {
        const auto lowToMid = juce::jlimit(20.0f, 19000.0f, lowToMidParam->load());
        const auto midToHigh = juce::jmin(20000.0f, juce::jmax(midToHighParam->load(), lowToMid * 1.01f));
        detectors[0].setFrequencyRange(20.0f, lowToMid);
        detectors[1].setFrequencyRange(lowToMid, midToHigh);
        detectors[2].setFrequencyRange(midToHigh, 20000.0f);

        for (size_t band = 0; band < numBands; ++band)
            players[band]->processBlock(splitMidiBuffers[band], midiMessages, audio.getNumSamples());

        for (size_t band = 0; band < numBands; ++band)
        {
            detectors[band].setSensitivity(sensitivityParams[band]->load());
            detectors[band].setVelocitySensitivity(velocitySensitivity);
            detectors[band].setDetectionMode(detectionMode);
            detectors[band].process(audio);
        }

        for (auto& player : players)
            player->finishBlock();
    }

    outputNoteActive.store(updateNoteState(outputNotes, midiMessages));
    audio.clear();
}

bool SynthGuitarAudioProcessor::updateNoteState(NoteState& notes, const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.getChannel() < 1 || message.getChannel() > 16)
            continue;
        auto& channelNotes = notes[static_cast<size_t>(message.getChannel() - 1)];
        if (message.isNoteOn())
            channelNotes.set(static_cast<size_t>(message.getNoteNumber()));
        else if (message.isNoteOff())
            channelNotes.reset(static_cast<size_t>(message.getNoteNumber()));
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            channelNotes.reset();
    }
    for (const auto& channelNotes : notes)
        if (channelNotes.any())
            return true;
    return false;
}

void SynthGuitarAudioProcessor::clearNoteActivity()
{
    for (auto& notes : inputNotes)
        notes.reset();
    for (auto& notes : outputNotes)
        notes.reset();
    inputNoteActive.store(false);
    outputNoteActive.store(false);
}

juce::AudioProcessorEditor* SynthGuitarAudioProcessor::createEditor()
{
    return new RenderedUserInterface(*this);
}

bool SynthGuitarAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String SynthGuitarAudioProcessor::getName() const
{
    return "Instrument";
}

bool SynthGuitarAudioProcessor::acceptsMidi() const
{
    return true;
}

bool SynthGuitarAudioProcessor::producesMidi() const
{
    return true;
}

bool SynthGuitarAudioProcessor::isMidiEffect() const
{
    return false;
}

double SynthGuitarAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SynthGuitarAudioProcessor::getNumPrograms()
{
    return 1;
}

int SynthGuitarAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SynthGuitarAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String SynthGuitarAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void SynthGuitarAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void SynthGuitarAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destinationData);
}

void SynthGuitarAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthGuitarAudioProcessor();
}
