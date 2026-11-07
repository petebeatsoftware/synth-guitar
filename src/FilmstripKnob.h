#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class FilmstripKnob final : public juce::Slider
{
public:
	FilmstripKnob(const juce::Image& image, int frames);
	void paint(juce::Graphics& graphics) override;

private:
	juce::Image filmstrip;
	int frameCount;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilmstripKnob)
};
