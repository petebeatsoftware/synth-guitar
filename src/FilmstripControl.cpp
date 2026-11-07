#include "FilmstripControl.h"

FilmstripControl::FilmstripControl(const juce::Image& image, int frames)
	: filmstrip(image), frameCount(juce::jmax(1, frames))
{
	setInterceptsMouseClicks(false, false);
}

void FilmstripControl::attachChoice(juce::RangedAudioParameter& parameter, std::vector<int> stateFrames)
{
	choiceFrames = std::move(stateFrames);
	setInterceptsMouseClicks(true, false);
	setWantsKeyboardFocus(true);
	attachment = std::make_unique<juce::ParameterAttachment>(parameter, [this](float value)
	{
		currentChoice = juce::jlimit(0, static_cast<int>(choiceFrames.size()) - 1, juce::roundToInt(value));
		setFrame(choiceFrames[static_cast<size_t>(currentChoice)]);
	});
	attachment->sendInitialUpdate();
}

void FilmstripControl::setFrame(int frame)
{
	frame = juce::jlimit(0, frameCount - 1, frame);
	if (currentFrame != frame)
	{
		currentFrame = frame;
		repaint();
	}
}

void FilmstripControl::paint(juce::Graphics& graphics)
{
	if (filmstrip.isNull() || filmstrip.getHeight() % frameCount != 0)
		return;
	const auto frameHeight = filmstrip.getHeight() / frameCount;
	graphics.drawImage(filmstrip, 0, 0, getWidth(), getHeight(),
					   0, currentFrame * frameHeight, filmstrip.getWidth(), frameHeight);
}

void FilmstripControl::advanceChoice()
{
	if (attachment != nullptr && !choiceFrames.empty())
		attachment->setValueAsCompleteGesture(static_cast<float>((currentChoice + 1) % static_cast<int>(choiceFrames.size())));
}

void FilmstripControl::mouseDown(const juce::MouseEvent& event)
{
	if (event.mods.isLeftButtonDown())
		advanceChoice();
}

bool FilmstripControl::keyPressed(const juce::KeyPress& key)
{
	if (attachment != nullptr && (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey))
	{
		advanceChoice();
		return true;
	}
	return false;
}
