#pragma once

#include <array>
#include <juce_audio_utils/juce_audio_utils.h>

#include "MidiTransposer.h"
#include "TransientDetector.h"

class SynthGuitarAudioProcessor final : public juce::AudioProcessor
{
public:
    SynthGuitarAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }
    bool isTransientNoteActive(int band) const noexcept { return detectors[static_cast<size_t>(band)].isNoteActive(); }

    static constexpr int numBands = 3;
    static constexpr const char* sensitivityParameterIds[numBands] = { "sensitivityLow", "sensitivityMid", "sensitivityHigh" };
    static constexpr const char* lowToMidParameterId = "lowToMid";
    static constexpr const char* midToHighParameterId = "midToHigh";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState parameters;
    std::array<std::atomic<float>*, numBands> sensitivityParams{};
    std::atomic<float>* lowToMidParam = nullptr;
    std::atomic<float>* midToHighParam = nullptr;
    std::array<instrument::TransientDetector, numBands> detectors;
    juce::MidiBuffer midiScratchBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthGuitarAudioProcessor)
};
