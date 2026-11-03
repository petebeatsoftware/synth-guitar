#include "SynthGuitarUserInterface.h"

#include "SynthGuitarAudioProcessor.h"

SynthGuitarUserInterface::SynthGuitarUserInterface(SynthGuitarAudioProcessor& processor)
    : AudioProcessorEditor(processor), audioProcessor(processor)
{
    titleLabel.setText("Pete Beat Synth Guitar", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    descriptionLabel.setText("Audio passthrough  |  Transient detector: C3",
                             juce::dontSendNotification);
    descriptionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(descriptionLabel);

    sensitivityLabel.setText("Sensitivity", juce::dontSendNotification);
    sensitivityLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(sensitivityLabel);

    sensitivitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    sensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    addAndMakeVisible(sensitivitySlider);
    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getParameters(), SynthGuitarAudioProcessor::sensitivityParameterId,
        sensitivitySlider);

    noteIndicatorLabel.setText("Note", juce::dontSendNotification);
    addAndMakeVisible(noteIndicatorLabel);

    setResizable(true, true);
    setResizeLimits(360, 200, 720, 320);
    setSize(480, 220);
    startTimerHz(30);
}

SynthGuitarUserInterface::~SynthGuitarUserInterface()
{
    stopTimer();
}

void SynthGuitarUserInterface::timerCallback()
{
    const auto active = audioProcessor.isTransientNoteActive();
    if (active != noteIndicatorActive)
    {
        noteIndicatorActive = active;
        repaint(noteIndicatorBounds.getSmallestIntegerContainer().expanded(2));
    }
}

void SynthGuitarUserInterface::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff171a21));
    graphics.setColour(juce::Colour(0xff67d5b5));
    graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 12.0f);
    graphics.setColour(juce::Colour(0xff171a21));
    graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(14.0f), 10.0f);

    graphics.setColour(noteIndicatorActive ? juce::Colour(0xff67d5b5) : juce::Colour(0xff2c313c));
    graphics.fillEllipse(noteIndicatorBounds);
    graphics.setColour(juce::Colour(0xff67d5b5));
    graphics.drawEllipse(noteIndicatorBounds, 1.5f);
}

void SynthGuitarUserInterface::resized()
{
    auto content = getLocalBounds().reduced(24);
    titleLabel.setBounds(content.removeFromTop(content.getHeight() / 3));
    descriptionLabel.setBounds(content.removeFromTop(content.getHeight() / 3));

    auto sensitivityRow = content.removeFromTop(content.getHeight() / 2);
    sensitivityLabel.setBounds(sensitivityRow.removeFromLeft(90));
    sensitivitySlider.setBounds(sensitivityRow);

    auto indicatorRow = content.withSizeKeepingCentre(80, juce::jmin(24, content.getHeight()));
    noteIndicatorBounds = indicatorRow.removeFromLeft(indicatorRow.getHeight()).toFloat().reduced(2.0f);
    noteIndicatorLabel.setBounds(indicatorRow);
}
