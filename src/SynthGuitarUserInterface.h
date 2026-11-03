#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class SynthGuitarAudioProcessor;

class SynthGuitarUserInterface final : public juce::AudioProcessorEditor
{
public:
    explicit SynthGuitarUserInterface(SynthGuitarAudioProcessor& processor);
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    juce::Label titleLabel;
    juce::Label descriptionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthGuitarUserInterface);
};
