#include "SynthGuitarAudioProcessor.h"

#include "SynthGuitarUserInterface.h"

SynthGuitarAudioProcessor::SynthGuitarAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void SynthGuitarAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    midiScratchBuffer.ensureSize(1024 * 1024);
}

void SynthGuitarAudioProcessor::releaseResources()
{
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

    instrument::transposeMidiDownOneSemitone(midiMessages, midiScratchBuffer);
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
    destinationData.reset();
}

void SynthGuitarAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthGuitarAudioProcessor();
}
