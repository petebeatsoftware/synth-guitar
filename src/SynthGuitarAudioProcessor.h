#pragma once

#include <array>
#include <bitset>
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
    bool isSingleNoteActive() const noexcept { return singlePlayer->isNoteActive(); }
    bool isInputNoteActive() const noexcept { return inputNoteActive.load(); }
    bool isOutputNoteActive() const noexcept { return outputNoteActive.load(); }
    juce::uint64 getTransientCount() const noexcept
    {
        return singleDetector.getTransientCount();
    }

    static constexpr const char* detectionModeParameterId = "detectionMode";
    static constexpr const char* singleSensitivityParameterId = "singleSensitivity";
    static constexpr const char* singleLowFrequencyParameterId = "singleLowFrequency";
    static constexpr const char* singleHighFrequencyParameterId = "singleHighFrequency";
    static constexpr const char* strumParameterId = "strum";
    static constexpr const char* velocitySensitivityParameterId = "velocitySensitivity";

private:
    static constexpr int numSplitBuffers = 3;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    using NoteState = std::array<std::bitset<128>, 16>;
    static bool updateNoteState(NoteState& notes, const juce::MidiBuffer& midi);
    void clearNoteActivity();

    juce::AudioProcessorValueTreeState parameters;
    NoteState inputNotes{};
    NoteState outputNotes{};
    std::atomic<bool> inputNoteActive{ false };
    std::atomic<bool> outputNoteActive{ false };
    std::atomic<float>* detectionModeParam = nullptr;
    std::atomic<float>* singleSensitivityParam = nullptr;
    std::atomic<float>* singleLowFrequencyParam = nullptr;
    std::atomic<float>* singleHighFrequencyParam = nullptr;
    std::atomic<float>* strumParam = nullptr;
    std::atomic<float>* velocitySensitivityParam = nullptr;
    std::shared_ptr<instrument::TriggeredPlayer> singlePlayer;
    instrument::TransientDetector singleDetector;
    int lastStrum = -1;
    juce::MidiBuffer singleInputBuffer;
    juce::MidiBuffer midiScratchBuffer;
    instrument::MidiSplitter midiSplitter;
    std::array<juce::MidiBuffer, numSplitBuffers> splitMidiBuffers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthGuitarAudioProcessor)
};
