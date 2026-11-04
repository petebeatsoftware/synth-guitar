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
    void updateModeVisibility();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    static constexpr int numBands = 3;
    static constexpr int numSliders = 5;

    SynthGuitarAudioProcessor& audioProcessor;
    juce::Label titleLabel;
    juce::Label descriptionLabel;
    juce::Label bandModeLabel;
    juce::ComboBox bandModeBox;
    std::unique_ptr<ComboBoxAttachment> bandModeAttachment;
    juce::Label modeLabel;
    juce::ComboBox modeBox;
    std::unique_ptr<ComboBoxAttachment> modeAttachment;
    juce::Label strumLabel;
    juce::ComboBox strumBox;
    std::unique_ptr<ComboBoxAttachment> strumAttachment;
    juce::Label velocitySensitivityLabel;
    juce::Slider velocitySensitivitySlider;
    std::unique_ptr<SliderAttachment> velocitySensitivityAttachment;
    std::array<juce::Label, numSliders> sliderLabels;
    std::array<juce::Slider, numSliders> sliders;
    std::array<std::unique_ptr<SliderAttachment>, numSliders> sliderAttachments;
    std::array<juce::Label, 3> singleSliderLabels;
    std::array<juce::Slider, 3> singleSliders;
    std::array<std::unique_ptr<SliderAttachment>, 3> singleSliderAttachments;
    std::array<juce::Label, numBands> noteIndicatorLabels;
    std::array<juce::Rectangle<float>, numBands> noteIndicatorBounds;
    std::array<bool, numBands> noteIndicatorActive{};
    juce::Label singleIndicatorLabel;
    juce::Rectangle<float> singleIndicatorBounds;
    bool singleIndicatorActive = false;
    int currentBandMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthGuitarUserInterface);
};
