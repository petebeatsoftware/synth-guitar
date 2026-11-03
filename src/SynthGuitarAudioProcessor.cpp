#include "SynthGuitarAudioProcessor.h"

#include "SynthGuitarUserInterface.h"

SynthGuitarAudioProcessor::SynthGuitarAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMS", createParameterLayout())
{
    for (size_t band = 0; band < numBands; ++band)
    {
        sensitivityParams[band] = parameters.getRawParameterValue(sensitivityParameterIds[band]);
        detectors[band].setNoteNumber(36 + 12 * static_cast<int>(band));
    }
    lowToMidParam = parameters.getRawParameterValue(lowToMidParameterId);
    midToHighParam = parameters.getRawParameterValue(midToHighParameterId);
    setLatencySamples(instrument::TransientDetector::kHopSize);
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

    auto frequencyRange = juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f);
    frequencyRange.setSkewForCentre(1000.0f);
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ lowToMidParameterId, 1 }, "Low to Mid", frequencyRange, 200.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ midToHighParameterId, 1 }, "Mid to High", frequencyRange, 1000.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));
    return layout;
}

void SynthGuitarAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (auto& detector : detectors)
        detector.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(instrument::TransientDetector::kHopSize);
    midiScratchBuffer.ensureSize(1024 * 1024);
}

void SynthGuitarAudioProcessor::releaseResources()
{
    for (auto& detector : detectors)
        detector.reset();
    midiScratchBuffer.clear();
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

    const auto lowToMid = juce::jlimit(20.0f, 19000.0f, lowToMidParam->load());
    const auto midToHigh = juce::jmin(20000.0f, juce::jmax(midToHighParam->load(), lowToMid * 1.01f));
    detectors[0].setFrequencyRange(20.0f, lowToMid);
    detectors[1].setFrequencyRange(lowToMid, midToHigh);
    detectors[2].setFrequencyRange(midToHigh, 20000.0f);

    midiMessages.clear();
    for (size_t band = 0; band < numBands; ++band)
    {
        detectors[band].setSensitivity(sensitivityParams[band]->load());
        detectors[band].process(audio, midiMessages);
    }
}

juce::AudioProcessorEditor* SynthGuitarAudioProcessor::createEditor()
{
    return new SynthGuitarUserInterface(*this);
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
