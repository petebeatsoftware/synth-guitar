#include "PluginProcessor.h"

#include "PluginEditor.h"

InstrumentAudioProcessor::InstrumentAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void InstrumentAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    midiScratchBuffer.ensureSize(1024 * 1024);
}

void InstrumentAudioProcessor::releaseResources()
{
    midiScratchBuffer.clear();
}

bool InstrumentAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return input == output
        && (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo());
}

void InstrumentAudioProcessor::processBlock(juce::AudioBuffer<float>& audio,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        audio.clear(channel, 0, audio.getNumSamples());

    instrument::transposeMidiDownOneSemitone(midiMessages, midiScratchBuffer);
}

juce::AudioProcessorEditor* InstrumentAudioProcessor::createEditor()
{
    return new InstrumentAudioProcessorEditor(*this);
}

bool InstrumentAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String InstrumentAudioProcessor::getName() const
{
    return "Instrument";
}

bool InstrumentAudioProcessor::acceptsMidi() const
{
    return true;
}

bool InstrumentAudioProcessor::producesMidi() const
{
    return true;
}

bool InstrumentAudioProcessor::isMidiEffect() const
{
    return true;
}

double InstrumentAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int InstrumentAudioProcessor::getNumPrograms()
{
    return 1;
}

int InstrumentAudioProcessor::getCurrentProgram()
{
    return 0;
}

void InstrumentAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String InstrumentAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void InstrumentAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void InstrumentAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    destinationData.reset();
}

void InstrumentAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InstrumentAudioProcessor();
}
