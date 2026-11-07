#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "TriggeredPlayer.h"

namespace instrument
{

/** Detects transients in audio and triggers MIDI notes.

	The input is band-limited by a 24dB/octave Butterworth high pass and low pass (two stacked
	TPT state variable stages each) before analysis.
	Hybrid mode triggers promptly on envelope attacks and uses spectral flux to catch
	onsets the envelope misses. Envelope-only and spectral-only modes are also available.
	Spectral detection needs a window of audio and has more latency than envelope detection;
	the hop size alone does not determine detection latency.
*/
class TransientDetector
{
public:
	enum class DetectionMode { Spectral, Envelope, Hybrid };

	static constexpr int kFftOrder = 9;
	static constexpr int kFftSize = 1 << kFftOrder;
	static constexpr int kHopSize = 64;

	explicit TransientDetector(std::shared_ptr<TriggeredPlayer> triggeredPlayer);

	void prepare(double sampleRate, int maximumBlockSize);

	/** Clears spectral and envelope detection state. Does not emit a note-off. */
	void reset();

	/** Sensitivity in the range 0 to 1. Higher values detect transients more eagerly. */
	void setSensitivity(float newSensitivity) noexcept;
	float getSensitivity() const noexcept { return sensitivity.load(); }

	/** Velocity sensitivity, 0 (always 127) to 1 (full dynamic range). */
	void setVelocitySensitivity(float value) noexcept { velocitySensitivity.store(juce::jlimit(0.0f, 1.0f, value)); }
	float getVelocitySensitivity() const noexcept { return velocitySensitivity.load(); }
	static juce::uint8 intensityToVelocity(float intensity, float sensitivity) noexcept;

	void setDetectionMode(DetectionMode newMode) noexcept { mode.store(newMode); }
	DetectionMode getDetectionMode() const noexcept { return mode.load(); }

	/** Sets the 24dB/octave high pass and low pass cutoffs (Hz) applied before detection. */
	void setFrequencyRange(float highPassHz, float lowPassHz) noexcept
	{
		highPassFrequency.store(highPassHz);
		lowPassFrequency.store(lowPassHz);
	}

	/** Analyses the input and triggers the player on each detected transient. */
	void process(const juce::AudioBuffer<float>& input);
	juce::uint64 getTransientCount() const noexcept { return transientCount.load(); }

private:
	bool analyseHop(float& intensityOut);
	bool processEnvelopeSample(float sample, float currentSensitivity) noexcept;

	std::shared_ptr<TriggeredPlayer> player;
	juce::dsp::FFT fft;
	static constexpr int kFilterStages = 2;
	std::array<juce::dsp::StateVariableTPTFilter<float>, kFilterStages> highPass;
	std::array<juce::dsp::StateVariableTPTFilter<float>, kFilterStages> lowPass;
	float appliedHighPass = -1.0f;
	float appliedLowPass = -1.0f;
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
	float fastEnv = 0.0f;
	float slowEnv = 0.0f;
	float fastAttackCoeff = 0.0f;
	float fastReleaseCoeff = 0.0f;
	float slowCoeff = 0.0f;

	double sampleRate = 44100.0;
	int refractorySamples = 0;
	int refractoryCounter = 0;
	int samplesSinceLastTrigger = kFftSize;

	std::atomic<float> sensitivity{ 0.5f };
	std::atomic<juce::uint64> transientCount{ 0 };
	std::atomic<float> velocitySensitivity{ 0.0f };
	std::atomic<DetectionMode> mode{ DetectionMode::Hybrid };
	std::atomic<float> highPassFrequency{ 20.0f };
	std::atomic<float> lowPassFrequency{ 20000.0f };
};

} // namespace instrument
