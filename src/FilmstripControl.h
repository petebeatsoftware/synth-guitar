#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <memory>
#include <vector>

class FilmstripControl final : public juce::Component
{
public:
	FilmstripControl(const juce::Image& image, int frames);
	void attachChoice(juce::RangedAudioParameter& parameter, std::vector<int> stateFrames);
	void setFrame(int frame);
	int getFrame() const noexcept { return currentFrame; }
	void paint(juce::Graphics& graphics) override;
	void mouseDown(const juce::MouseEvent& event) override;

private:
	void advanceChoice();
	juce::Image filmstrip;
	int frameCount;
	int currentFrame = 0;
	int currentChoice = 0;
	std::vector<int> choiceFrames;
	std::unique_ptr<juce::ParameterAttachment> attachment;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilmstripControl)
};
