#include "FilmstripKnob.h"

FilmstripKnob::FilmstripKnob(const juce::Image& image, int frames)
	: filmstrip(image), frameCount(juce::jmax(1, frames))
{
	setSliderStyle(juce::Slider::RotaryVerticalDrag);
	setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}

void FilmstripKnob::paint(juce::Graphics& graphics)
{
	if (filmstrip.isNull() || filmstrip.getHeight() % frameCount != 0)
		return;

	const auto proportion = juce::jlimit(0.0, 1.0, valueToProportionOfLength(getValue()));
	const auto frame = juce::roundToInt(proportion * static_cast<double>(frameCount - 1));
	const auto frameHeight = filmstrip.getHeight() / frameCount;
	graphics.drawImage(filmstrip, 0, 0, getWidth(), getHeight(),
					   0, frame * frameHeight, filmstrip.getWidth(), frameHeight);
}
