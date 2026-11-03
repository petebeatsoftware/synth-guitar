#include "TransientDetector.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
int transientFailures = 0;

void expectTransient(bool condition, const char* description)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << description << '\n';
		++transientFailures;
	}
}

struct Event
{
	long long position;
	bool isOn;
	int note;
};

constexpr double kRate = 48000.0;
constexpr int kBlock = 512;

/** Runs the detector over a signal of silence with noise bursts at the given sample positions. */
std::vector<Event> run(instrument::TransientDetector& detector, long long totalSamples,
					   const std::vector<long long>& burstStarts, float amplitude)
{
	std::vector<Event> events;
	juce::Random random(1234);
	juce::AudioBuffer<float> buffer(1, kBlock);

	for (long long start = 0; start < totalSamples; start += kBlock)
	{
		for (int i = 0; i < kBlock; ++i)
		{
			float value = 0.0f;
			for (const auto burst : burstStarts)
				if (start + i >= burst && start + i < burst + 480)
					value = amplitude * (random.nextFloat() * 2.0f - 1.0f);
			buffer.setSample(0, i, value);
		}

		juce::MidiBuffer midi;
		detector.process(buffer, midi);
		for (const auto metadata : midi)
		{
			const auto message = metadata.getMessage();
			events.push_back({ start + metadata.samplePosition, message.isNoteOn(),
							   message.getNoteNumber() });
		}
	}
	return events;
}

void testSilenceProducesNoMidi()
{
	instrument::TransientDetector detector;
	detector.prepare(kRate, kBlock);
	const auto events = run(detector, 48000, {}, 0.0f);
	expectTransient(events.empty(), "silence produces no MIDI");
	expectTransient(!detector.isNoteActive(), "no note is active after silence");
}

void testBurstTriggersNoteOnThenTimedNoteOff()
{
	instrument::TransientDetector detector;
	detector.prepare(kRate, kBlock);
	const auto events = run(detector, 48000, { 24000 }, 0.5f);

	expectTransient(events.size() == 2, "one burst produces a note-on and a note-off");
	if (events.size() == 2)
	{
		expectTransient(events[0].isOn && events[0].note == 48, "first event is note-on for C3 (48)");
		expectTransient(!events[1].isOn && events[1].note == 48, "second event is note-off for C3");
		expectTransient(events[0].position >= 24000 && events[0].position < 24000 + 1024,
						"note-on occurs shortly after the burst");
		expectTransient(events[1].position - events[0].position == 9600,
						"note-off occurs 0.2 s after note-on");
	}
	expectTransient(!detector.isNoteActive(), "note is inactive after note-off");
}

void testIsNoteActiveWhileNoteHeld()
{
	instrument::TransientDetector detector;
	detector.prepare(kRate, kBlock);
	run(detector, 24000 + 2048, { 24000 }, 0.5f);
	expectTransient(detector.isNoteActive(), "note is active shortly after a transient");
}

void testRetriggerSendsNoteOffBeforeNoteOn()
{
	instrument::TransientDetector detector;
	detector.prepare(kRate, kBlock);
	const auto events = run(detector, 48000, { 24000, 24000 + 4800 }, 0.5f);

	expectTransient(events.size() == 4, "two close transients produce four events");
	if (events.size() == 4)
	{
		expectTransient(events[0].isOn, "first event is note-on");
		expectTransient(!events[1].isOn && events[2].isOn, "note-off precedes the retriggered note-on");
		expectTransient(events[1].position == events[2].position,
						"retrigger note-off and note-on share a sample position");
		expectTransient(!events[3].isOn, "final event is note-off");
	}
}

void testSensitivityControlsDetection()
{
	instrument::TransientDetector low;
	low.prepare(kRate, kBlock);
	low.setSensitivity(0.0f);
	const auto lowEvents = run(low, 48000, { 24000 }, 0.005f);
	instrument::TransientDetector high;
	high.prepare(kRate, kBlock);
	high.setSensitivity(1.0f);
	const auto highEvents = run(high, 48000, { 24000 }, 0.005f);

	expectTransient(lowEvents.empty(), "low sensitivity ignores a quiet transient");
	expectTransient(!highEvents.empty(), "high sensitivity detects a quiet transient");
}

void testTonalNoteOnsetIsDetected()
{
	instrument::TransientDetector detector;
	detector.prepare(kRate, kBlock);

	bool detected = false;
	juce::AudioBuffer<float> buffer(1, kBlock);
	for (long long start = 0; start < 48000; start += kBlock)
	{
		for (int i = 0; i < kBlock; ++i)
		{
			const auto n = start + i;
			float value = 0.0f;
			if (n >= 24000)
			{
				const auto t = static_cast<double>(n - 24000) / kRate;
				const auto decay = static_cast<float>(std::exp(-3.0 * t));
				for (int harmonic = 1; harmonic <= 4; ++harmonic)
					value += 0.05f * decay * static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * 220.0 * harmonic * t));
			}
			buffer.setSample(0, i, value);
		}
		juce::MidiBuffer midi;
		detector.process(buffer, midi);
		for (const auto metadata : midi)
			detected = detected || metadata.getMessage().isNoteOn();
	}
	expectTransient(detected, "a tonal (piano-like) note onset triggers a note-on at default sensitivity");
}
}

int runTransientDetectorTests()
{
	testSilenceProducesNoMidi();
	testBurstTriggersNoteOnThenTimedNoteOff();
	testIsNoteActiveWhileNoteHeld();
	testRetriggerSendsNoteOffBeforeNoteOn();
	testSensitivityControlsDetection();
	testTonalNoteOnsetIsDetected();

	if (transientFailures != 0)
		std::cerr << transientFailures << " transient detector test assertion(s) failed\n";
	else
		std::cout << "All transient detector tests passed\n";

	return transientFailures;
}
