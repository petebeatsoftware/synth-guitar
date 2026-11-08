#include "MidiSplitter.h"
#include "TestHelpers.h"

#include <array>
#include <vector>

namespace
{
using Outputs = std::array<juce::MidiBuffer, instrument::MidiSplitter::kNumOutputs>;

juce::MidiBuffer makeBuffer(std::initializer_list<std::pair<juce::MidiMessage, int>> events)
{
	juce::MidiBuffer buffer;
	for (const auto& event : events)
		buffer.addEvent(event.first, event.second);
	return buffer;
}

std::vector<std::pair<int, juce::MidiMessage>> eventsOf(const juce::MidiBuffer& buffer)
{
	std::vector<std::pair<int, juce::MidiMessage>> events;
	for (const auto event : buffer)
		events.emplace_back(event.samplePosition, event.getMessage());
	return events;
}

juce::MidiMessage on(int note) { return juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(90)); }
juce::MidiMessage off(int note) { return juce::MidiMessage::noteOff(1, note); }

class MidiSplitterTests final : public test::Suite
{
public:
	MidiSplitterTests() : Suite("MidiSplitter") {}

void testRouting()
{
	instrument::MidiSplitter splitter;
	Outputs out;

	check("First note routes to low output only", [&]
	{
		splitter.processData(makeBuffer({ { on(60), 5 } }), out);
		expect(eventsOf(out[0]).size() == 1 && eventsOf(out[0])[0].second.isNoteOn(), "first note goes to buffer 1");
		expect(out[1].isEmpty(), "first note not in buffer 2");
	});

	check("Higher note routes to upper output only", [&]
	{
		splitter.processData(makeBuffer({ { on(64), 7 } }), out);
		expect(out[0].isEmpty(), "higher note not in buffer 1");
		expect(eventsOf(out[1]).size() == 1, "higher note goes to buffer 2");
	});

	check("Lower note displaces prior low note at the same sample", [&]
	{
		splitter.processData(makeBuffer({ { on(50), 9 } }), out);
		const auto low = eventsOf(out[0]);
		expect(low.size() == 2 && low[0].second.isNoteOff() && low[0].second.getNoteNumber() == 60
				   && low[1].second.isNoteOn() && low[1].second.getNoteNumber() == 50,
			   "new lower note moves old low note out of buffer 1");
		const auto up = eventsOf(out[1]);
		expect(up.size() == 1 && up[0].first == 9 && up[0].second.isNoteOn()
				   && up[0].second.getNoteNumber() == 60,
			   "old low note moves to buffer 2 at same position");
	});

	check("Note-offs follow their routed outputs", [&]
	{
		splitter.processData(makeBuffer({ { off(64), 1 }, { off(50), 2 }, { off(60), 3 } }), out);
		expect(eventsOf(out[0]).size() == 1 && eventsOf(out[0])[0].second.getNoteNumber() == 50,
			   "low note-off goes to buffer 1 only");
		expect(eventsOf(out[1]).size() == 2, "other note-offs go to buffer 2");
	});
}

void runTest() override
{
	testRouting();
	check("Lowest note release does not promote upper notes", [&]
	{
		instrument::MidiSplitter splitter;
		Outputs out;
		splitter.processData(makeBuffer({ { on(40), 0 }, { on(60), 1 }, { off(40), 2 } }), out);
		splitter.processData(makeBuffer({ { off(60), 0 } }), out);
		expect(out[0].isEmpty() && !out[1].isEmpty(), "next-lowest note is not promoted");
	});
	check("Velocity-zero releases and other messages broadcast", [&]
	{
		instrument::MidiSplitter splitter;
		Outputs out;
		const auto zero = juce::MidiMessage::noteOn(1, 40, static_cast<juce::uint8>(0));
		const auto cc = juce::MidiMessage::controllerEvent(1, 64, 127);
		const auto bend = juce::MidiMessage::pitchWheel(1, 9000);
		splitter.processData(makeBuffer({ { on(40), 0 }, { zero, 1 }, { cc, 2 }, { bend, 3 } }), out);
		expect(eventsOf(out[0]).size() == 4, "velocity-0 note-on follows the low note and other data in buffer 1");
		expect(eventsOf(out[1]).size() == 2, "other data goes to all buffers");

		splitter.processData(makeBuffer({ { on(70), 0 } }), out);
		expect(!out[0].isEmpty() && out[1].isEmpty(), "note after low note released becomes lowest");
	});
	check("Reset clears held notes", [&]
	{
		instrument::MidiSplitter splitter;
		Outputs out;
		splitter.processData(makeBuffer({ { on(40), 0 } }), out);
		splitter.reset();
		splitter.processData(makeBuffer({ { on(60), 0 } }), out);
		expect(!out[0].isEmpty() && out[1].isEmpty(), "reset clears held notes");
	});
	check("Empty input clears every output without clearing held notes", [&]
	{
		instrument::MidiSplitter splitter;
		Outputs out;
		splitter.processData(makeBuffer({ { on(40), 0 } }), out);
		splitter.processData({}, out);
		for (const auto& output : out)
			expect(output.isEmpty());
		juce::MidiBuffer held;
		splitter.addHeldNotes(0, held, 12);
		expectEvent(eventsOf(held), 0, 12, 40, true);
	});
	check("Held-note replay preserves channel velocity partition and timestamp", [&]
	{
		instrument::MidiSplitter splitter;
		Outputs out;
		splitter.processData(makeBuffer({ { test::on(40, 3, 73), 0 },
			{ test::on(60, 4, 81), 1 }, { test::on(70, 5, 95), 2 }, { test::off(70, 5), 3 } }), out);
		for (int output = 0; output < instrument::MidiSplitter::kNumOutputs; ++output)
		{
			juce::MidiBuffer held;
			splitter.addHeldNotes(output, held, 17);
			const auto events = eventsOf(held);
			expectEquals(static_cast<int>(events.size()), 1);
			expectEvent(events, 0, 17, output == 0 ? 40 : 60, true, output == 0 ? 3 : 4);
			if (!events.empty())
				expectEquals(static_cast<int>(events[0].second.getVelocity()), output == 0 ? 73 : 81);
		}
		splitter.reset();
		juce::MidiBuffer held;
		splitter.addHeldNotes(0, held, 0);
		splitter.addHeldNotes(1, held, 0);
		expect(held.isEmpty());
	});
}
};

static MidiSplitterTests midiSplitterTests;
}
