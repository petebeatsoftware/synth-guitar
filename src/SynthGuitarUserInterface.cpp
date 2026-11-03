#include "SynthGuitarUserInterface.h"

#include "SynthGuitarAudioProcessor.h"

SynthGuitarUserInterface::SynthGuitarUserInterface(SynthGuitarAudioProcessor& processor)
    : AudioProcessorEditor(processor), audioProcessor(processor)
{
    titleLabel.setText("Pete Beat Synth Guitar", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    descriptionLabel.setText("Audio passthrough  |  Transient detectors: Low C2, Mid C3, High C4",
                             juce::dontSendNotification);
    descriptionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(descriptionLabel);

    const char* labels[numSliders] = { "Low to Mid", "Mid to High", "Low Sens.", "Mid Sens.", "High Sens." };
    const char* ids[numSliders] = {
        SynthGuitarAudioProcessor::lowToMidParameterId,
        SynthGuitarAudioProcessor::midToHighParameterId,
        SynthGuitarAudioProcessor::sensitivityParameterIds[0],
        SynthGuitarAudioProcessor::sensitivityParameterIds[1],
        SynthGuitarAudioProcessor::sensitivityParameterIds[2] };
    for (size_t i = 0; i < numSliders; ++i)
    {
        sliderLabels[i].setText(labels[i], juce::dontSendNotification);
        sliderLabels[i].setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(sliderLabels[i]);

        sliders[i].setSliderStyle(juce::Slider::LinearHorizontal);
        sliders[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        addAndMakeVisible(sliders[i]);
        sliderAttachments[i] = std::make_unique<SliderAttachment>(processor.getParameters(), ids[i], sliders[i]);
    }

    const char* bandNames[numBands] = { "Low", "Mid", "High" };
    for (size_t i = 0; i < numBands; ++i)
    {
        noteIndicatorLabels[i].setText(bandNames[i], juce::dontSendNotification);
        addAndMakeVisible(noteIndicatorLabels[i]);
    }

    setResizable(true, true);
    setResizeLimits(400, 360, 720, 520);
    setSize(480, 380);
    startTimerHz(30);
}

SynthGuitarUserInterface::~SynthGuitarUserInterface()
{
    stopTimer();
}

void SynthGuitarUserInterface::timerCallback()
{
    for (int i = 0; i < numBands; ++i)
    {
        const auto index = static_cast<size_t>(i);
        const auto active = audioProcessor.isTransientNoteActive(i);
        if (active != noteIndicatorActive[index])
        {
            noteIndicatorActive[index] = active;
            repaint(noteIndicatorBounds[index].getSmallestIntegerContainer().expanded(2));
        }
    }
}

void SynthGuitarUserInterface::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff171a21));
    graphics.setColour(juce::Colour(0xff67d5b5));
    graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 12.0f);
    graphics.setColour(juce::Colour(0xff171a21));
    graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(14.0f), 10.0f);

    for (size_t i = 0; i < numBands; ++i)
    {
        graphics.setColour(noteIndicatorActive[i] ? juce::Colour(0xff67d5b5) : juce::Colour(0xff2c313c));
        graphics.fillEllipse(noteIndicatorBounds[i]);
        graphics.setColour(juce::Colour(0xff67d5b5));
        graphics.drawEllipse(noteIndicatorBounds[i], 1.5f);
    }
}

void SynthGuitarUserInterface::resized()
{
    auto content = getLocalBounds().reduced(24);
    titleLabel.setBounds(content.removeFromTop(48));
    descriptionLabel.setBounds(content.removeFromTop(24));

    auto indicatorRow = content.removeFromBottom(32);
    const auto cellWidth = indicatorRow.getWidth() / numBands;
    for (size_t i = 0; i < numBands; ++i)
    {
        auto cell = indicatorRow.removeFromLeft(cellWidth).withSizeKeepingCentre(80, 24);
        noteIndicatorBounds[i] = cell.removeFromLeft(cell.getHeight()).toFloat().reduced(2.0f);
        noteIndicatorLabels[i].setBounds(cell);
    }

    const auto rowHeight = content.getHeight() / numSliders;
    for (size_t i = 0; i < numSliders; ++i)
    {
        auto row = content.removeFromTop(rowHeight);
        sliderLabels[i].setBounds(row.removeFromLeft(90));
        sliders[i].setBounds(row);
    }
}
