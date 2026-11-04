#include "TriggeredPlayer.h"
#include "TestHelpers.h"

namespace
{
class TriggeredPlayerTests final : public test::Suite
{
public:
	TriggeredPlayerTests() : Suite("TriggeredPlayer") {}

	void runTest() override
	{
		check("Defaults and channel limits", [&]
		{
			instrument::TriggeredPlayer player;
			expectEquals(player.getChannel(), 1);
			expectEquals(player.getNoteLengthSeconds(), 0.2);
			expect(!player.isNoteActive());
			player.setChannel(0);
			expectEquals(player.getChannel(), 1);
			player.setChannel(17);
			expectEquals(player.getChannel(), 16);
		});
		check("Held notes and controllers produce no output without a trigger", [&]
		{
			instrument::TriggeredPlayer player;
			player.prepare(1000.0, 64);
			auto input = test::buffer({ { test::on(60), 0 },
				{ juce::MidiMessage::controllerEvent(1, 64, 127), 1 } });
			juce::MidiBuffer output;
			player.trigger(0);
			player.processBlock(input, output, 64);
			player.finishBlock();
			expect(output.isEmpty());
			expect(!player.isNoteActive());
		});
		check("Trigger plays only notes held at its sample with maximum velocity and channel", [&]
		{
			instrument::TriggeredPlayer player;
			player.prepare(1000.0, 64);
			player.setChannel(7);
			auto input = test::buffer({ { test::on(60), 4 }, { test::on(64), 8 }, { test::on(67), 20 } });
			juce::MidiBuffer output;
			player.processBlock(input, output, 64);
			player.trigger(10);
			player.finishBlock();
			const auto events = test::eventsOf(output);
			expectEquals(static_cast<int>(events.size()), 2);
			expectEvent(events, 0, 10, 60, true, 7);
			expectEvent(events, 1, 10, 64, true, 7);
			for (const auto& event : events)
				expectEquals(static_cast<int>(event.second.getVelocity()), 127);
			expect(player.isNoteActive());
		});
		for (const int velocity : { 0, 1, 42, 127, 255 })
			check("Trigger velocity " + juce::String(velocity), [&]
			{
				instrument::TriggeredPlayer player;
				player.prepare(1000.0, 64);
				auto input = test::buffer({ { test::on(60), 0 }, { test::on(64), 0 } });
				juce::MidiBuffer output;
				player.processBlock(input, output, 64);
				player.trigger(0, static_cast<juce::uint8>(velocity));
				player.finishBlock();
				expectEquals(output.getNumEvents(), 2);
				for (const auto& event : test::eventsOf(output))
				{
					expect(event.second.isNoteOn());
					expectEquals(static_cast<int>(event.second.getVelocity()), juce::jlimit(1, 127, velocity));
				}
			});
		for (const bool zeroVelocity : { false, true })
			check(zeroVelocity ? "Velocity-zero releases a playing note" : "Input note-off releases a playing note", [&]
			{
				instrument::TriggeredPlayer player;
				player.prepare(1000.0, 64);
				auto input = test::buffer({ { test::on(60), 0 },
					{ zeroVelocity ? test::on(60, 1, 0) : test::off(60), 20 } });
				juce::MidiBuffer output;
				player.processBlock(input, output, 64);
				player.trigger(0);
				player.finishBlock();
				const auto events = test::eventsOf(output);
				expectEquals(static_cast<int>(events.size()), 2);
				expectEvent(events, 0, 0, 60, true);
				expectEvent(events, 1, 20, 60, false);
				expect(!player.isNoteActive());
			});
		check("Retrigger stops chord before restarting it", [&]
		{
			instrument::TriggeredPlayer player;
			player.prepare(1000.0, 64);
			auto input = test::buffer({ { test::on(60), 0 }, { test::on(64), 0 } });
			juce::MidiBuffer output;
			player.processBlock(input, output, 64);
			player.trigger(0);
			player.trigger(30);
			player.finishBlock();
			const auto events = test::eventsOf(output);
			expectEquals(static_cast<int>(events.size()), 6);
			expectEvent(events, 2, 30, 60, false);
			expectEvent(events, 3, 30, 64, false);
			expectEvent(events, 4, 30, 60, true);
			expectEvent(events, 5, 30, 64, true);
		});
		for (const int duration : { 1, 20, 64, 70, 128 })
			check("Timer expires after exactly " + juce::String(duration) + " samples", [&]
			{
				instrument::TriggeredPlayer player;
				player.prepare(1000.0, 64);
				player.setNoteLengthSeconds(static_cast<double>(duration) / 1000.0);
				auto input = test::buffer({ { test::on(60), 0 } });
				std::vector<test::Event> absoluteEvents;
				for (int block = 0; block < 3; ++block)
				{
					juce::MidiBuffer output;
					player.processBlock(input, output, 64);
					if (block == 0)
						player.trigger(0);
					player.finishBlock();
					for (auto event : test::eventsOf(output))
					{
						event.first += block * 64;
						absoluteEvents.push_back(event);
					}
					input.clear();
				}
				expectEquals(static_cast<int>(absoluteEvents.size()), 2);
				expectEvent(absoluteEvents, 1, duration, 60, false);
				expect(!player.isNoteActive());
			});
		check("Nonpositive note length is at least one sample and trigger position is clamped", [&]
		{
			instrument::TriggeredPlayer player;
			player.prepare(1000.0, 64);
			player.setNoteLengthSeconds(-1.0);
			auto input = test::buffer({ { test::on(60), 0 } });
			juce::MidiBuffer output;
			player.processBlock(input, output, 64);
			player.trigger(-5);
			player.finishBlock();
			auto events = test::eventsOf(output);
			expectEquals(static_cast<int>(events.size()), 2);
			expectEvent(events, 0, 0, 60, true);
			expectEvent(events, 1, 1, 60, false);
			output.clear();
			player.processBlock({}, output, 64);
			player.trigger(100);
			player.finishBlock();
			events = test::eventsOf(output);
			expectEvent(events, 0, 63, 60, true);
		});
		for (const bool release : { false, true })
			check(release ? "Release emits note-offs and clears held notes" : "Reset silently clears all state", [&]
			{
				instrument::TriggeredPlayer player;
				player.prepare(1000.0, 64);
				auto input = test::buffer({ { test::on(60), 0 } });
				juce::MidiBuffer output;
				player.processBlock(input, output, 64);
				player.trigger(0);
				player.finishBlock();
				output.clear();
				if (release)
					player.releaseNotes(output);
				else
					player.reset();
				expect(!player.isNoteActive());
				if (release)
				{
					expectEquals(output.getNumEvents(), 1);
					expectEvent(test::eventsOf(output), 0, 0, 60, false);
				}
				else
					expect(output.isEmpty());
				output.clear();
				player.processBlock({}, output, 64);
				player.trigger(0);
				player.finishBlock();
				expect(output.isEmpty());
			});
		check("Invalid sample rate uses 44100 Hz fallback", [&]
		{
			instrument::TriggeredPlayer player;
			player.prepare(0.0, 512);
			player.setNoteLengthSeconds(0.01);
			auto input = test::buffer({ { test::on(60), 0 } });
			juce::MidiBuffer output;
			player.processBlock(input, output, 512);
			player.trigger(0);
			player.finishBlock();
			expectEvent(test::eventsOf(output), 1, 441, 60, false);
		});
	}
};

static TriggeredPlayerTests triggeredPlayerTests;
}
