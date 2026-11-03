#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <memory>
#include <vector>

namespace instrument
{

/** Detects transients in audio using spectral flux and emits MIDI.

	A transient produces a note-on (C3). The matching note-off is sent either just before the
	next transient's note-on, or once kNoteLengthSeconds have elapsed.
	Detection latency is up to one hop (kHopSize samples).
*/
class TransientDetector
{
public:
	static constexpr int kNoteNumber = 48; // C3
	static constexpr int kChannel = 1;
	static constexpr juce::uint8 kVelocity = 100;
	static constexpr double kNoteLengthSeconds = 0.2;
	static constexpr int kFftOrder = 10;
	static constexpr int kFftSize = 1 << kFftOrder;
	static constexpr int kHopSize = 256;

	TransientDetector();

	void prepare(double sampleRate, int maximumBlockSize);

	/** Clears all detection state. Does not emit a note-off. */
	void reset();

	/** Sensitivity in the range 0 to 1. Higher values detect transients more eagerly. */
	void setSensitivity(float newSensitivity) noexcept;
	float getSensitivity() const noexcept { return sensitivity.load(); }

	/** Analyses the input and appends note-on/note-off events to midiOut. */
	void process(const juce::AudioBuffer<float>& input, juce::MidiBuffer& midiOut);

	/** True while a note-on has been sent without a corresponding note-off. */
	bool isNoteActive() const noexcept { return noteActive.load(); }

private:
	bool analyseHop();
	void sendNoteOn(juce::MidiBuffer& midiOut, int samplePosition);
	void sendNoteOff(juce::MidiBuffer& midiOut, int samplePosition);

	juce::dsp::FFT fft;
	juce::dsp::WindowingFunction<float> window;

	std::vector<float> fifo;
	std::vector<float> fftData;
	std::vector<float> previousMagnitudes;
	int fifoWritePos = 0;
	int samplesUntilHop = kHopSize;
	bool fifoFilled = false;

	float previousFlux = 0.0f;
	float fluxMean = 0.0f;
	float meanCoefficient = 0.0f;

	double sampleRate = 44100.0;
	int refractorySamples = 0;
	int refractoryCounter = 0;
	int noteLengthSamples = 0;
	int samplesSinceNoteOn = 0;

	std::atomic<float> sensitivity{ 0.5f };
	std::atomic<bool> noteActive{ false };
};

} // namespace instrument
