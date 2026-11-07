#pragma once

#include "FilmstripKnob.h"
#include "FilmstripControl.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include <memory>
#include <vector>

class SynthGuitarAudioProcessor;

class RenderedUserInterface final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
	explicit RenderedUserInterface(SynthGuitarAudioProcessor& processor);
	~RenderedUserInterface() override;
	void paint(juce::Graphics& graphics) override;
	void resized() override;

private:
	class Content final : public juce::Component
	{
	public:
		void paint(juce::Graphics& graphics) override;
		juce::Image background;
		juce::StringArray errors;
	};

	void loadAssets(const juce::File& directory);
	void timerCallback() override;

	using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
	static constexpr int designWidth = 800;
	static constexpr int designHeight = 1080;

	SynthGuitarAudioProcessor& audioProcessor;
	Content content;
	std::vector<std::unique_ptr<FilmstripKnob>> knobs;
	std::vector<std::unique_ptr<FilmstripControl>> controls;
	FilmstripControl* detectLight = nullptr;
	FilmstripControl* midiInLight = nullptr;
	FilmstripControl* midiOutLight = nullptr;
	FilmstripControl* strings = nullptr;
	juce::uint64 lastTransientCount = 0;
	double detectUntil = 0.0;
	std::vector<std::unique_ptr<SliderAttachment>> attachments;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RenderedUserInterface)
};
