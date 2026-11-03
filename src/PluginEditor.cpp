#include "PluginEditor.h"

#include "PluginProcessor.h"

InstrumentAudioProcessorEditor::InstrumentAudioProcessorEditor(InstrumentAudioProcessor& processor)
    : AudioProcessorEditor(processor)
{
    titleLabel.setText("Instrument", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    descriptionLabel.setText("Audio passthrough  |  MIDI transpose: -1 semitone",
                             juce::dontSendNotification);
    descriptionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(descriptionLabel);

    setResizable(true, true);
    setResizeLimits(360, 140, 720, 280);
    setSize(480, 180);
}

void InstrumentAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff171a21));
    graphics.setColour(juce::Colour(0xff67d5b5));
    graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 12.0f);
    graphics.setColour(juce::Colour(0xff171a21));
    graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(14.0f), 10.0f);
}

void InstrumentAudioProcessorEditor::resized()
{
    auto content = getLocalBounds().reduced(24);
    titleLabel.setBounds(content.removeFromTop(content.getHeight() / 2));
    descriptionLabel.setBounds(content);
}
