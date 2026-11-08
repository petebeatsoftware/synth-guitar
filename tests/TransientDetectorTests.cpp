#include "TransientDetector.h"
#include "TestHelpers.h"

#include <cmath>
#include <vector>

namespace
{
struct Event
{
	long long position;
	bool isOn;
	int note;
	int velocity;
};

constexpr double kRate = 48000.0;
constexpr int kBlock = 512;

/** Detector with its player, holding note 48 from the first block. */
struct Rig
{
	Rig() : player(std::make_shared<instrument::TriggeredPlayer>()), detector(player) {}

	void prepare()
	{
		player->prepare(kRate, kBlock);
		detector.prepare(kRate, kBlock);
	}

	void processBlock(const juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool first)
	{
		juce::MidiBuffer input;
		if (first)
			input.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8) 100), 0);
		player->processBlock(input, midi, buffer.getNumSamples());
		detector.process(buffer);
		player->finishBlock();
	}

	std::shared_ptr<instrument::TriggeredPlayer> player;
	instrument::TransientDetector detector;
};

/** Runs the detector over a signal of silence with noise bursts at the given sample positions. */
std::vector<Event> run(Rig& rig, long long totalSamples,
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
		rig.processBlock(buffer, midi, start == 0);
		for (const auto metadata : midi)
		{
			const auto message = metadata.getMessage();
			events.push_back({ start + metadata.samplePosition, message.isNoteOn(),
							   message.getNoteNumber(), static_cast<int>(message.getVelocity()) });
		}
	}
	return events;
}

template <typename Signal>
std::vector<Event> runSignal(Rig& rig, long long totalSamples, Signal signal)
{
	std::vector<Event> events;
	juce::AudioBuffer<float> buffer(1, kBlock);
	for (long long start = 0; start < totalSamples; start += kBlock)
	{
		for (int i = 0; i < kBlock; ++i)
			buffer.setSample(0, i, signal(start + i));
		juce::MidiBuffer midi;
		rig.processBlock(buffer, midi, start == 0);
		for (const auto metadata : midi)
		{
			const auto message = metadata.getMessage();
			events.push_back({ start + metadata.samplePosition, message.isNoteOn(), message.getNoteNumber(),
				static_cast<int>(message.getVelocity()) });
		}
	}
	return events;
}

std::vector<long long> noteOns(const std::vector<Event>& events)
{
	std::vector<long long> positions;
	for (const auto& event : events)
		if (event.isOn)
			positions.push_back(event.position);
	return positions;
}

class TransientDetectorTests final : public test::Suite
{
public:
	TransientDetectorTests() : Suite("TransientDetector") {}

	void expectTransient(bool condition, const char* description)
	{
		expect(condition, description);
	}

void testSilenceProducesNoMidi()
{
	for (const auto mode : { instrument::TransientDetector::DetectionMode::Spectral,
							instrument::TransientDetector::DetectionMode::Envelope,
							instrument::TransientDetector::DetectionMode::Hybrid })
	{
		check("Silence produces no MIDI in mode " + juce::String(static_cast<int>(mode)), [&]
		{
			Rig detector;
			detector.prepare();
			detector.detector.setDetectionMode(mode);
			const auto events = run(detector, 48000, {}, 0.0f);
			expectTransient(events.empty(), "silence produces no MIDI in any mode");
			expectTransient(!detector.player->isNoteActive(), "no note is active after silence");
		});
	}
}

void runTest() override
{
	check("Velocity mapping endpoints interpolation and clamping", [&]
	{
		using Detector = instrument::TransientDetector;
		for (int i = 0; i <= 100; ++i)
		{
			const float intensity = static_cast<float>(i) / 100.0f;
			expectEquals(static_cast<int>(Detector::intensityToVelocity(intensity, 0.0f)), 127);
			if (i > 0)
				expect(Detector::intensityToVelocity(intensity, 1.0f)
					>= Detector::intensityToVelocity(intensity - 0.01f, 1.0f));
		}
		expectEquals(static_cast<int>(Detector::intensityToVelocity(0.0f, 1.0f)), 1);
		expectEquals(static_cast<int>(Detector::intensityToVelocity(1.0f, 1.0f)), 127);
		expectEquals(static_cast<int>(Detector::intensityToVelocity(0.0f, 0.5f)), 64);
		expectEquals(static_cast<int>(Detector::intensityToVelocity(-1.0f, 2.0f)), 1);
		expectEquals(static_cast<int>(Detector::intensityToVelocity(2.0f, -1.0f)), 127);
	});
	for (const auto mode : { instrument::TransientDetector::DetectionMode::Envelope,
		instrument::TransientDetector::DetectionMode::Spectral,
		instrument::TransientDetector::DetectionMode::Hybrid })
		check("Velocity follows transient intensity in mode " + juce::String(static_cast<int>(mode)), [&]
		{
			int velocities[2]{};
			for (int level = 0; level < 2; ++level)
			{
				Rig rig;
				rig.prepare();
				rig.detector.setDetectionMode(mode);
				rig.detector.setSensitivity(1.0f);
				rig.detector.setVelocitySensitivity(1.0f);
				const auto events = run(rig, 48000, { 24000 }, level == 0 ? 0.02f : 0.8f);
				for (const auto& event : events)
					if (event.isOn && velocities[level] == 0)
						velocities[level] = event.velocity;
				expect(velocities[level] > 0);
			}
			expect(velocities[1] > velocities[0], "Louder bursts produce higher velocity");
			Rig fixed;
			fixed.prepare();
			fixed.detector.setDetectionMode(mode);
			const auto events = run(fixed, 48000, { 24000 }, 0.5f);
			expect(!noteOns(events).empty());
			for (const auto& event : events)
				if (event.isOn)
					expectEquals(event.velocity, 127);
		});
	testSilenceProducesNoMidi();
	check("Mode timing and hybrid deduplication", [&]
	{
		constexpr long long onset = 24000;
		auto click = [onset](long long sample) { return sample >= onset && sample < onset + 48 ? 0.7f : 0.0f; };
		Rig hybrid;
		hybrid.prepare();
		const auto hybridOns = noteOns(runSignal(hybrid, 48000, click));
		Rig envelope;
		envelope.prepare();
		envelope.detector.setDetectionMode(instrument::TransientDetector::DetectionMode::Envelope);
		const auto envelopeOns = noteOns(runSignal(envelope, 48000, click));
		Rig spectral;
		spectral.prepare();
		spectral.detector.setDetectionMode(instrument::TransientDetector::DetectionMode::Spectral);
		const auto spectralOns = noteOns(runSignal(spectral, 48000, click));
		expectTransient(hybridOns.size() == 1, "hybrid click triggers once without delayed spectral duplicate");
		expectTransient(envelopeOns.size() == 1, "envelope click triggers once");
		expectTransient(spectralOns.size() == 1, "spectral click triggers once");
		if (hybridOns.size() == 1 && envelopeOns.size() == 1)
		{
			expectTransient(hybridOns[0] == envelopeOns[0], "hybrid preserves immediate envelope timing");
			expectTransient(hybridOns[0] >= onset && hybridOns[0] < onset + 144,
							"hybrid triggers within 3 ms of click");
		}
		if (spectralOns.size() == 1 && envelopeOns.size() == 1)
			expectTransient(spectralOns[0] > envelopeOns[0], "spectral click follows envelope click");
	});
	check("Close clicks share refractory period", [&]
	{
		Rig rig;
		rig.prepare();
		const auto ons = noteOns(runSignal(rig, 48000, [](long long sample)
		{
			return (sample >= 24000 && sample < 24048) || (sample >= 25000 && sample < 25048)
				? 0.7f : 0.0f;
		}));
		expectTransient(ons.size() == 1, "clicks within 50 ms share a refractory period");
	});
	check("Sustained tone does not retrigger", [&]
	{
		Rig rig;
		rig.prepare();
		const auto ons = noteOns(runSignal(rig, 48000, [](long long sample)
		{
			return sample < 24000 ? 0.0f : 0.3f * static_cast<float>(std::sin(
				2.0 * juce::MathConstants<double>::pi * 440.0 * (sample - 24000) / kRate));
		}));
		expectTransient(ons.size() == 1, "sustained tone does not retrigger");
	});
	check("Reset matches fresh envelope and spectral state", [&]
	{
		Rig reused;
		reused.prepare();
		run(reused, 48000, { 24000 }, 0.5f);
		reused.player->reset();
		reused.detector.reset();
		Rig fresh;
		fresh.prepare();
		const auto reusedEvents = run(reused, 48000, { 24000 }, 0.5f);
		const auto freshEvents = run(fresh, 48000, { 24000 }, 0.5f);
		expectTransient(reusedEvents.size() == freshEvents.size(), "reset matches fresh event count");
		if (reusedEvents.size() == freshEvents.size())
			for (size_t i = 0; i < reusedEvents.size(); ++i)
				expectTransient(reusedEvents[i].position == freshEvents[i].position
					&& reusedEvents[i].isOn == freshEvents[i].isOn, "reset matches fresh event timing");
	});
	check("Spectral path catches envelope-missed onset", [&]
	{
		auto fade = [](long long sample)
		{
			if (sample < 24000 || sample >= 25440)
				return 0.0f;
			const auto elapsed = sample - 24000;
			auto noise = static_cast<juce::uint32>(elapsed) * 1664525u + 1013904223u;
			noise ^= noise >> 16;
			const auto gain = 0.035f * static_cast<float>(elapsed) / 1440.0f;
			return gain * (2.0f * static_cast<float>(noise & 0xffffu) / 65535.0f - 1.0f);
		};
		Rig envelope;
		envelope.prepare();
		envelope.detector.setDetectionMode(instrument::TransientDetector::DetectionMode::Envelope);
		const auto envelopeOns = noteOns(runSignal(envelope, 48000, fade));
		Rig hybrid;
		hybrid.prepare();
		const auto hybridOns = noteOns(runSignal(hybrid, 48000, fade));
		expectTransient(envelopeOns.empty(), "slow fade does not cross envelope attack threshold");
		expectTransient(hybridOns.size() == 1, "spectral path catches onset missed by envelope");
	});
	check("Burst produces timed note-off", [&]
	{
		Rig detector;
		detector.prepare();
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
		expectTransient(!detector.player->isNoteActive(), "note is inactive after note-off");
	});
	check("Active state while note held", [&]
	{
		Rig detector;
		detector.prepare();
		run(detector, 24000 + 2048, { 24000 }, 0.5f);
		expectTransient(detector.player->isNoteActive(), "note is active shortly after a transient");
	});
	check("Retrigger sends note-off before note-on", [&]
	{
		Rig detector;
		detector.prepare();
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
	});
	check("Sensitivity controls quiet transient detection", [&]
	{
		Rig low;
		low.prepare();
		low.detector.setSensitivity(0.0f);
		const auto lowEvents = run(low, 48000, { 24000 }, 0.005f);
		Rig high;
		high.prepare();
		high.detector.setSensitivity(1.0f);
		const auto highEvents = run(high, 48000, { 24000 }, 0.005f);

		expectTransient(lowEvents.empty(), "low sensitivity ignores a quiet transient");
		expectTransient(!highEvents.empty(), "high sensitivity detects a quiet transient");
	});
	check("Tonal onset detection", [&]
	{
		Rig detector;
		detector.prepare();

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
			detector.processBlock(buffer, midi, start == 0);
			for (const auto metadata : midi)
				detected = detected || metadata.getMessage().isNoteOn();
		}
		expectTransient(detected, "a tonal (piano-like) note onset triggers a note-on at default sensitivity");
	});
	check("Configuration defaults and sensitivity limits", [&]
	{
		Rig rig;
		expectEquals(rig.detector.getSensitivity(), 0.5f);
		expectEquals(rig.detector.getVelocitySensitivity(), 0.0f);
		rig.detector.setVelocitySensitivity(-1.0f);
		expectEquals(rig.detector.getVelocitySensitivity(), 0.0f);
		rig.detector.setVelocitySensitivity(2.0f);
		expectEquals(rig.detector.getVelocitySensitivity(), 1.0f);
		expect(rig.detector.getDetectionMode() == instrument::TransientDetector::DetectionMode::Hybrid);
		rig.detector.setSensitivity(-1.0f);
		expectEquals(rig.detector.getSensitivity(), 0.0f);
		rig.detector.setSensitivity(2.0f);
		expectEquals(rig.detector.getSensitivity(), 1.0f);
	});
	check("Empty buffers do not trigger", [&]
	{
		Rig rig;
		rig.prepare();
		juce::AudioBuffer<float> noChannels;
		juce::AudioBuffer<float> noSamples(1, 0);
		rig.detector.process(noChannels);
		rig.detector.process(noSamples);
		const auto events = run(rig, 48000, {}, 0.0f);
		expect(events.empty());
	});
	check("Opposite stereo channels cancel before detection", [&]
	{
		Rig rig;
		rig.prepare();
		juce::AudioBuffer<float> audio(2, kBlock);
		for (int i = 0; i < kBlock; ++i)
		{
			audio.setSample(0, i, 0.7f);
			audio.setSample(1, i, -0.7f);
		}
		juce::MidiBuffer midi;
		rig.processBlock(audio, midi, true);
		expect(midi.isEmpty());
	});
	for (const auto rate : { 44100.0, 96000.0 })
		check("Detection at sample rate " + juce::String(rate), [&]
		{
			Rig rig;
			rig.player->prepare(rate, kBlock);
			rig.detector.prepare(rate, kBlock);
			rig.detector.setFrequencyRange(-10.0f, 100000.0f);
			const auto events = noteOns(run(rig, 48000, { 24000 }, 0.5f));
			expectEquals(static_cast<int>(events.size()), 1);
		});
}
};

static TransientDetectorTests transientDetectorTests;
}
