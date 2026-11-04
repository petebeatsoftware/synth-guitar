#pragma once

#include <array>
#include <juce_audio_utils/juce_audio_utils.h>

#include "MidiSplitter.h"
#include "MidiTransposer.h"
#include "TransientDetector.h"
#include "TriggeredPlayer.h"

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
    bool isTransientNoteActive(int band) const noexcept { return players[static_cast<size_t>(band)]->isNoteActive(); }
    bool isSingleNoteActive() const noexcept { return singlePlayer->isNoteActive(); }

    static constexpr int numBands = 3;
    static constexpr const char* sensitivityParameterIds[numBands] = { "sensitivityLow", "sensitivityMid", "sensitivityHigh" };
    static constexpr const char* lowToMidParameterId = "lowToMid";
    static constexpr const char* midToHighParameterId = "midToHigh";
    static constexpr const char* detectionModeParameterId = "detectionMode";
    static constexpr const char* bandModeParameterId = "bandMode";
    static constexpr const char* singleSensitivityParameterId = "singleSensitivity";
    static constexpr const char* singleLowFrequencyParameterId = "singleLowFrequency";
    static constexpr const char* singleHighFrequencyParameterId = "singleHighFrequency";
    static constexpr const char* strumParameterId = "strum";
    static constexpr const char* velocitySensitivityParameterId = "velocitySensitivity";

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState parameters;
    std::array<std::atomic<float>*, numBands> sensitivityParams{};
    std::atomic<float>* lowToMidParam = nullptr;
    std::atomic<float>* midToHighParam = nullptr;
    std::atomic<float>* detectionModeParam = nullptr;
    std::atomic<float>* bandModeParam = nullptr;
    std::atomic<float>* singleSensitivityParam = nullptr;
    std::atomic<float>* singleLowFrequencyParam = nullptr;
    std::atomic<float>* singleHighFrequencyParam = nullptr;
    std::atomic<float>* strumParam = nullptr;
    std::atomic<float>* velocitySensitivityParam = nullptr;
    std::array<std::shared_ptr<instrument::TriggeredPlayer>, numBands> players;
    std::array<instrument::TransientDetector, numBands> detectors;
    std::shared_ptr<instrument::TriggeredPlayer> singlePlayer;
    instrument::TransientDetector singleDetector;
    int lastBandMode = -1;
    int lastStrum = -1;
    juce::MidiBuffer singleInputBuffer;
    juce::MidiBuffer midiScratchBuffer;
    instrument::MidiSplitter midiSplitter;
    std::array<juce::MidiBuffer, numBands> splitMidiBuffers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthGuitarAudioProcessor)
};
