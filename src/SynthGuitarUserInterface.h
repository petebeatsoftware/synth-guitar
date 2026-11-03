#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <memory>

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

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    static constexpr int numBands = 3;
    static constexpr int numSliders = 5;

    SynthGuitarAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label descriptionLabel;
    std::array<juce::Label, numSliders> sliderLabels;
    std::array<juce::Slider, numSliders> sliders;
    std::array<std::unique_ptr<SliderAttachment>, numSliders> sliderAttachments;
    std::array<juce::Label, numBands> noteIndicatorLabels;
    std::array<juce::Rectangle<float>, numBands> noteIndicatorBounds;
    std::array<bool, numBands> noteIndicatorActive{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthGuitarUserInterface);
};
