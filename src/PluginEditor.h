#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class InstrumentAudioProcessor;

class InstrumentAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit InstrumentAudioProcessorEditor(InstrumentAudioProcessor& processor);
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label descriptionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentAudioProcessorEditor)
};
