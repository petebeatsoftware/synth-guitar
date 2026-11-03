#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

class SynthGuitarAudioProcessor;

class SynthGuitarUserInterface final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SynthGuitarUserInterface(SynthGuitarAudioProcessor& processor);
    ~SynthGuitarUserInterface() override;
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;

    SynthGuitarAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label descriptionLabel;
    juce::Label sensitivityLabel;
    juce::Slider sensitivitySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    juce::Label noteIndicatorLabel;
    juce::Rectangle<float> noteIndicatorBounds;
    bool noteIndicatorActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthGuitarUserInterface);
};
