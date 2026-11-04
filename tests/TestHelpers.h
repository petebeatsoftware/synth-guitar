#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>

namespace test
{
using Event = std::pair<int, juce::MidiMessage>;

inline juce::MidiBuffer buffer(std::initializer_list<std::pair<juce::MidiMessage, int>> events)
{
	juce::MidiBuffer result;
	for (const auto& event : events)
		result.addEvent(event.first, event.second);
	return result;
}

inline std::vector<Event> eventsOf(const juce::MidiBuffer& midi)
{
	std::vector<Event> result;
	for (const auto event : midi)
		result.emplace_back(event.samplePosition, event.getMessage());
	return result;
}

inline juce::MidiMessage on(int note, int channel = 1, int velocity = 90)
{
	return juce::MidiMessage::noteOn(channel, note, static_cast<juce::uint8>(velocity));
}

inline juce::MidiMessage off(int note, int channel = 1)
{
	return juce::MidiMessage::noteOff(channel, note);
}

inline void setParameter(juce::AudioProcessorValueTreeState& state, const char* id, float value)
{
	auto* parameter = state.getParameter(id);
	jassert(parameter != nullptr);
	parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

class Suite : public juce::UnitTest
{
public:
	explicit Suite(const juce::String& suiteName) : juce::UnitTest(suiteName, suiteName) {}

	void check(const juce::String& behavior, const std::function<void()>& body)
	{
		beginTest(behavior);
		body();
	}

	void expectEvent(const std::vector<Event>& events, size_t index, int position,
					 int note, bool isOn, int channel = 1)
	{
		expect(index < events.size(), "Expected event exists");
		if (index >= events.size())
			return;
		expectEquals(events[index].first, position, "Sample position");
		expectEquals(events[index].second.getNoteNumber(), note, "Note number");
		expectEquals(events[index].second.getChannel(), channel, "Channel");
		expect(isOn ? events[index].second.isNoteOn() : events[index].second.isNoteOff(), "Event type");
	}
};
}
