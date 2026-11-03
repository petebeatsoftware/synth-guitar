#include "MidiTransposer.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

namespace
{
int failures = 0;

void expect(bool condition, const char* description)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void addMessage(juce::MidiBuffer& buffer, const juce::MidiMessage& message, int samplePosition)
{
    expect(buffer.addEvent(message, samplePosition), "MIDI test event is added");
}

void testMidiTranspositionAndPreservation()
{
    juce::MidiBuffer midi;
    juce::MidiBuffer scratch;
    const auto noteOn = juce::MidiMessage::noteOn(2, 61, static_cast<juce::uint8>(100));
    const auto noteOff = juce::MidiMessage::noteOff(2, 61);
    const auto velocityZeroNoteOn = juce::MidiMessage::noteOn(1, 49, static_cast<juce::uint8>(0));
    const auto highestNote = juce::MidiMessage::noteOn(16, 127, static_cast<juce::uint8>(127));
    const auto controller = juce::MidiMessage::controllerEvent(1, 64, 127);
    const std::array<juce::uint8, 3> sysexData { 0xf0, 0x7d, 0xf7 };
    const auto sysex = juce::MidiMessage(sysexData.data(), static_cast<int>(sysexData.size()));

    addMessage(midi, noteOn, 11);
    addMessage(midi, controller, 19);
    addMessage(midi, noteOff, 30);
    addMessage(midi, velocityZeroNoteOn, 41);
    addMessage(midi, sysex, 52);
    addMessage(midi, highestNote, 63);

    instrument::transposeMidiDownOneSemitone(midi, scratch);

    std::vector<std::pair<int, juce::MidiMessage>> events;
    for (const auto event : midi)
        events.emplace_back(event.samplePosition, event.getMessage());

    expect(events.size() == 6, "all MIDI events are retained");
    if (events.size() == 6)
    {
        expect(events[0].first == 11
                   && events[0].second.getNoteNumber() == 60
                   && events[0].second.getChannel() == 2
                   && events[0].second.getVelocity() == 100,
               "note-on is transposed and keeps its timestamp");
        expect(events[1].first == 19
                   && events[1].second.getRawDataSize() == controller.getRawDataSize()
                   && std::equal(events[1].second.getRawData(),
                                 events[1].second.getRawData() + controller.getRawDataSize(),
                                 controller.getRawData()),
               "controller event is unchanged");
        expect(events[2].first == 30 && events[2].second.getNoteNumber() == 60,
               "note-off is transposed and keeps its timestamp");
        expect(events[3].first == 41 && events[3].second.getNoteNumber() == 48
                   && events[3].second.isNoteOn(true),
               "velocity-zero note-on is transposed and retained as note-on");
        expect(events[4].first == 52
                   && events[4].second.getRawDataSize() == sysex.getRawDataSize()
                   && std::equal(events[4].second.getRawData(),
                                 events[4].second.getRawData() + sysex.getRawDataSize(),
                                 sysex.getRawData()),
               "SysEx event is retained");
        expect(events[5].first == 63 && events[5].second.getNoteNumber() == 126,
               "highest MIDI note transposes without exceeding the range");
    }
}

void testNoteRangeClampsAtZero()
{
    juce::MidiBuffer midi;
    juce::MidiBuffer scratch;
    addMessage(midi, juce::MidiMessage::noteOn(1, 0, static_cast<juce::uint8>(80)), 0);

    instrument::transposeMidiDownOneSemitone(midi, scratch);

    const auto event = *midi.begin();
    expect(event.getMessage().getNoteNumber() == 0,
           "notes below the MIDI range clamp to the lowest MIDI note");
}

void testAudioPassesThroughUnchanged()
{
    InstrumentAudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);

    juce::AudioBuffer<float> audio(2, 128);
    std::array<std::array<float, 128>, 2> original {};
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        auto* samples = audio.getWritePointer(channel);
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            samples[sample] = std::sin(static_cast<float>((sample + channel * 17) * 0.13));
            original[static_cast<std::size_t>(channel)][static_cast<std::size_t>(sample)] = samples[sample];
        }
    }

    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);

    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
        {
            expect(audio.getSample(channel, sample)
                       == original[static_cast<std::size_t>(channel)][static_cast<std::size_t>(sample)],
                   "audio sample passes through unchanged");
        }
    }
}
}

int main()
{
    testMidiTranspositionAndPreservation();
    testNoteRangeClampsAtZero();
    testAudioPassesThroughUnchanged();

    if (failures != 0)
    {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All MIDI transposer tests passed\n";
    return 0;
}
