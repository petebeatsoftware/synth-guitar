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

    bandModeLabel.setText("Mode", juce::dontSendNotification);
    bandModeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bandModeLabel);
    bandModeBox.addItemList({ "Single", "Multi" }, 1);
    addAndMakeVisible(bandModeBox);
    bandModeAttachment = std::make_unique<ComboBoxAttachment>(
        processor.getParameters(), SynthGuitarAudioProcessor::bandModeParameterId, bandModeBox);

    modeLabel.setText("Detection", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(modeLabel);
    modeBox.addItemList({ "Spectral", "Envelope", "Hybrid" }, 1);
    addAndMakeVisible(modeBox);
    modeAttachment = std::make_unique<ComboBoxAttachment>(
        processor.getParameters(), SynthGuitarAudioProcessor::detectionModeParameterId, modeBox);

    strumLabel.setText("Strum", juce::dontSendNotification);
    strumLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(strumLabel);
    strumBox.addItemList({ "Low", "High" }, 1);
    addAndMakeVisible(strumBox);
    strumAttachment = std::make_unique<ComboBoxAttachment>(
        processor.getParameters(), SynthGuitarAudioProcessor::strumParameterId, strumBox);

    velocitySensitivityLabel.setText("Velocity Sensitivity", juce::dontSendNotification);
    velocitySensitivityLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(velocitySensitivityLabel);
    velocitySensitivitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    velocitySensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    velocitySensitivitySlider.setTextValueSuffix(" %");
    addAndMakeVisible(velocitySensitivitySlider);
    velocitySensitivityAttachment = std::make_unique<SliderAttachment>(
        processor.getParameters(), SynthGuitarAudioProcessor::velocitySensitivityParameterId,
        velocitySensitivitySlider);

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

    const char* singleLabels[3] = { "Low Freq.", "High Freq.", "Sensitivity" };
    const char* singleIds[3] = {
        SynthGuitarAudioProcessor::singleLowFrequencyParameterId,
        SynthGuitarAudioProcessor::singleHighFrequencyParameterId,
        SynthGuitarAudioProcessor::singleSensitivityParameterId };
    for (size_t i = 0; i < singleSliders.size(); ++i)
    {
        singleSliderLabels[i].setText(singleLabels[i], juce::dontSendNotification);
        singleSliderLabels[i].setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(singleSliderLabels[i]);
        singleSliders[i].setSliderStyle(juce::Slider::LinearHorizontal);
        singleSliders[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
        addAndMakeVisible(singleSliders[i]);
        singleSliderAttachments[i] = std::make_unique<SliderAttachment>(
            processor.getParameters(), singleIds[i], singleSliders[i]);
    }

    const char* bandNames[numBands] = { "Low", "Mid", "High" };
    for (size_t i = 0; i < numBands; ++i)
    {
        noteIndicatorLabels[i].setText(bandNames[i], juce::dontSendNotification);
        addAndMakeVisible(noteIndicatorLabels[i]);
    }

    singleIndicatorLabel.setText("Single", juce::dontSendNotification);
    addAndMakeVisible(singleIndicatorLabel);

    setResizable(true, true);
    setResizeLimits(400, 400, 720, 560);
    setSize(480, 420);
    updateModeVisibility();
    startTimerHz(30);
}

SynthGuitarUserInterface::~SynthGuitarUserInterface()
{
    stopTimer();
}

void SynthGuitarUserInterface::timerCallback()
{
    const auto bandMode = static_cast<int>(audioProcessor.getParameters().getRawParameterValue(
        SynthGuitarAudioProcessor::bandModeParameterId)->load());
    if (bandMode != currentBandMode)
        updateModeVisibility();

    if (currentBandMode == 0)
    {
        const auto active = audioProcessor.isSingleNoteActive();
        if (active != singleIndicatorActive)
        {
            singleIndicatorActive = active;
            repaint(singleIndicatorBounds.getSmallestIntegerContainer().expanded(2));
        }
        return;
    }

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

void SynthGuitarUserInterface::updateModeVisibility()
{
    currentBandMode = static_cast<int>(audioProcessor.getParameters().getRawParameterValue(
        SynthGuitarAudioProcessor::bandModeParameterId)->load());
    const bool single = currentBandMode == 0;
    descriptionLabel.setText(single ? "Audio passthrough  |  Single-band transient detector"
                                    : "Audio passthrough  |  Transient detectors: Low C2, Mid C3, High C4",
                             juce::dontSendNotification);
    strumLabel.setVisible(single);
    strumBox.setVisible(single);
    singleIndicatorLabel.setVisible(single);
    for (size_t i = 0; i < singleSliders.size(); ++i)
    {
        singleSliderLabels[i].setVisible(single);
        singleSliders[i].setVisible(single);
    }
    for (size_t i = 0; i < numSliders; ++i)
    {
        sliderLabels[i].setVisible(!single);
        sliders[i].setVisible(!single);
    }
    for (auto& label : noteIndicatorLabels)
        label.setVisible(!single);
    resized();
    repaint();
}

void SynthGuitarUserInterface::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff171a21));
    graphics.setColour(juce::Colour(0xff67d5b5));
    graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 12.0f);
    graphics.setColour(juce::Colour(0xff171a21));
    graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(14.0f), 10.0f);

    if (currentBandMode == 0)
    {
        graphics.setColour(singleIndicatorActive ? juce::Colour(0xff67d5b5) : juce::Colour(0xff2c313c));
        graphics.fillEllipse(singleIndicatorBounds);
        graphics.setColour(juce::Colour(0xff67d5b5));
        graphics.drawEllipse(singleIndicatorBounds, 1.5f);
    }
    else
    {
        for (size_t i = 0; i < numBands; ++i)
        {
            graphics.setColour(noteIndicatorActive[i] ? juce::Colour(0xff67d5b5) : juce::Colour(0xff2c313c));
            graphics.fillEllipse(noteIndicatorBounds[i]);
            graphics.setColour(juce::Colour(0xff67d5b5));
            graphics.drawEllipse(noteIndicatorBounds[i], 1.5f);
        }
    }
}

void SynthGuitarUserInterface::resized()
{
    auto content = getLocalBounds().reduced(24);
    titleLabel.setBounds(content.removeFromTop(48));
    descriptionLabel.setBounds(content.removeFromTop(24));

    auto bandModeRow = content.removeFromTop(36);
    bandModeLabel.setBounds(bandModeRow.removeFromLeft(90));
    bandModeBox.setBounds(bandModeRow);

    auto modeRow = content.removeFromTop(36);
    modeLabel.setBounds(modeRow.removeFromLeft(90));
    modeBox.setBounds(modeRow);

    if (currentBandMode == 0)
    {
        auto strumRow = content.removeFromTop(36);
        strumLabel.setBounds(strumRow.removeFromLeft(90));
        strumBox.setBounds(strumRow);
    }

    auto velocityRow = content.removeFromTop(36);
    velocitySensitivityLabel.setBounds(velocityRow.removeFromLeft(150));
    velocitySensitivitySlider.setBounds(velocityRow);

    auto indicatorRow = content.removeFromBottom(32);
    if (currentBandMode == 0)
    {
        auto cell = indicatorRow.withSizeKeepingCentre(80, 24);
        singleIndicatorBounds = cell.removeFromLeft(cell.getHeight()).toFloat().reduced(2.0f);
        singleIndicatorLabel.setBounds(cell);
    }
    else
    {
        const auto cellWidth = indicatorRow.getWidth() / numBands;
        for (size_t i = 0; i < numBands; ++i)
        {
            auto cell = indicatorRow.removeFromLeft(cellWidth).withSizeKeepingCentre(80, 24);
            noteIndicatorBounds[i] = cell.removeFromLeft(cell.getHeight()).toFloat().reduced(2.0f);
            noteIndicatorLabels[i].setBounds(cell);
        }
    }

    const auto rowCount = currentBandMode == 0 ? singleSliders.size() : sliders.size();
    const auto rowHeight = content.getHeight() / static_cast<int>(rowCount);
    for (size_t i = 0; i < rowCount; ++i)
    {
        auto row = content.removeFromTop(rowHeight);
        if (currentBandMode == 0)
        {
            singleSliderLabels[i].setBounds(row.removeFromLeft(90));
            singleSliders[i].setBounds(row);
        }
        else
        {
            sliderLabels[i].setBounds(row.removeFromLeft(90));
            sliders[i].setBounds(row);
        }
    }
}
