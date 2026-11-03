#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <memory>
#include <vector>

namespace instrument
{

/** Detects transients in audio using spectral flux and emits MIDI.

	The input is band-limited by a 24dB/octave high pass and low pass before analysis.
	A transient produces a note-on (see setNoteNumber). The matching
	note-off is sent either just before the next transient's note-on, or
	Detection latency is up to one hop (kHopSize samples).
*/
class TransientDetector
{
public:
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

	/** Note number emitted on detection. Takes effect on the next note-on. */
	void setNoteNumber(int newNoteNumber) noexcept { noteNumber.store(juce::jlimit(0, 127, newNoteNumber)); }
	int getNoteNumber() const noexcept { return noteNumber.load(); }

	/** Sets the 24dB/octave high pass and low pass cutoffs (Hz) applied before detection. */
	void setFrequencyRange(float highPassHz, float lowPassHz) noexcept
	{
		highPassFrequency.store(highPassHz);
		lowPassFrequency.store(lowPassHz);
	}

	/** Analyses the input and appends note-on/note-off events to midiOut. */
	void process(const juce::AudioBuffer<float>& input, juce::MidiBuffer& midiOut);

	/** True while a note-on has been sent without a corresponding note-off. */
	bool isNoteActive() const noexcept { return noteActive.load(); }

private:
	bool analyseHop();
	void sendNoteOn(juce::MidiBuffer& midiOut, int samplePosition);
	void sendNoteOff(juce::MidiBuffer& midiOut, int samplePosition);

	juce::dsp::FFT fft;
	juce::dsp::LinkwitzRileyFilter<float> highPass;
	juce::dsp::LinkwitzRileyFilter<float> lowPass;
	float appliedHighPass = -1.0f;
	float appliedLowPass = -1.0f;
	int activeNoteNumber = 48;
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
	std::atomic<int> noteNumber{ 48 };
	std::atomic<float> highPassFrequency{ 20.0f };
	std::atomic<float> lowPassFrequency{ 20000.0f };
	std::atomic<bool> noteActive{ false };
};

} // namespace instrument
