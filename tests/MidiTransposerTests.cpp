#include "MidiTransposer.h"
#include "TestHelpers.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace
{
class MidiTransposerTests final : public test::Suite
{
public:
    MidiTransposerTests() : Suite("MidiTransposer") {}

    void runTest() override
    {
        for (const auto& entry : std::vector<std::pair<juce::String, juce::MidiMessage>> {
                 { "Note-on", test::on(61, 2, 100) },
                 { "Note-off", test::off(61, 2) },
                 { "Velocity-zero note-on", test::on(49, 1, 0) },
                 { "Upper boundary", test::on(127, 16, 127) },
                 { "Lower boundary note-on", test::on(0, 1, 80) },
                 { "Lower boundary note-off", test::off(0) } })
        {
            check(entry.first + " transposes and preserves other bytes and timing", [&]
            {
                auto midi = test::buffer({ { entry.second, 11 } });
                juce::MidiBuffer scratch;
                instrument::transposeMidiDownOneSemitone(midi, scratch);
                const auto events = test::eventsOf(midi);
                expectEquals(static_cast<int>(events.size()), 1);
                if (events.size() != 1)
                    return;
                expectEquals(events[0].first, 11);
                auto expected = entry.second;
                expected.setNoteNumber(juce::jmax(0, expected.getNoteNumber() - 1));
                expect(std::equal(expected.getRawData(), expected.getRawData() + expected.getRawDataSize(),
                                  events[0].second.getRawData()), "Only note number changes");
            });
        }

        const std::array<juce::uint8, 3> sysexData { 0xf0, 0x7d, 0xf7 };
        for (const auto& message : { juce::MidiMessage::controllerEvent(1, 64, 127),
                                    juce::MidiMessage(sysexData.data(), static_cast<int>(sysexData.size())),
                                    juce::MidiMessage::pitchWheel(3, 9000) })
        {
            check(message.isSysEx() ? "SysEx is unchanged" : message.isController()
                  ? "Controller is unchanged" : "Pitch wheel is unchanged", [&]
            {
                auto midi = test::buffer({ { message, 52 } });
                juce::MidiBuffer scratch;
                instrument::transposeMidiDownOneSemitone(midi, scratch);
                const auto events = test::eventsOf(midi);
                expectEquals(static_cast<int>(events.size()), 1);
                if (events.size() != 1)
                    return;
                expectEquals(events[0].first, 52);
                expectEquals(events[0].second.getRawDataSize(), message.getRawDataSize());
                expect(std::equal(message.getRawData(), message.getRawData() + message.getRawDataSize(),
                                  events[0].second.getRawData()));
            });
        }

        check("Mixed events retain count and order", [&]
        {
            const auto controller = juce::MidiMessage::controllerEvent(1, 64, 127);
            const juce::MidiMessage sysex(sysexData.data(), static_cast<int>(sysexData.size()));
            auto midi = test::buffer({ { test::on(61, 2, 100), 11 }, { controller, 19 },
                                      { test::off(61, 2), 30 }, { test::on(49, 1, 0), 41 },
                                      { sysex, 52 }, { test::on(127, 16, 127), 63 } });
            juce::MidiBuffer scratch;
            instrument::transposeMidiDownOneSemitone(midi, scratch);
            const auto events = test::eventsOf(midi);
            expectEquals(static_cast<int>(events.size()), 6);
            expectEvent(events, 0, 11, 60, true, 2);
            expectEvent(events, 2, 30, 60, false, 2);
            expectEvent(events, 3, 41, 48, false);
            expectEvent(events, 5, 63, 126, true, 16);
            if (events.size() == 6)
            {
                expectEquals(events[1].first, 19);
                expect(events[1].second.isController());
                expectEquals(events[4].first, 52);
                expect(events[4].second.isSysEx());
            }
        });

        check("Empty input clears stale scratch data", [&]
        {
            juce::MidiBuffer midi;
            auto scratch = test::buffer({ { test::on(60), 0 } });
            instrument::transposeMidiDownOneSemitone(midi, scratch);
            expect(midi.isEmpty());
            midi.addEvent(test::on(60), 3);
            instrument::transposeMidiDownOneSemitone(midi, scratch);
            const auto events = test::eventsOf(midi);
            expectEquals(static_cast<int>(events.size()), 1);
            expectEvent(events, 0, 3, 59, true);
        });
    }
};

static MidiTransposerTests midiTransposerTests;
}
