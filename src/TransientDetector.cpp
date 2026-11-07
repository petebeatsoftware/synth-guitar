#include "TransientDetector.h"

#include <algorithm>
#include <cmath>

namespace instrument
{

namespace
{
constexpr float kCompression = 100.0f;
constexpr float kFluxFullScale = 20.0f;
constexpr float kIntensityFloorDb = -60.0f;
}

TransientDetector::TransientDetector(std::shared_ptr<TriggeredPlayer> triggeredPlayer)
	: player(std::move(triggeredPlayer)),
	  fft(kFftOrder),
	  window(static_cast<size_t>(kFftSize), juce::dsp::WindowingFunction<float>::hann, false),
	  fifo(static_cast<size_t>(kFftSize), 0.0f),
	  fftData(static_cast<size_t>(kFftSize) * 2, 0.0f),
	  previousMagnitudes(static_cast<size_t>(kFftSize / 2 + 1), 0.0f)
{
}

void TransientDetector::prepare(double newSampleRate, int maximumBlockSize)
{
	juce::ignoreUnused(maximumBlockSize);
	sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
	highPass.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
	lowPass.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
	const juce::dsp::ProcessSpec spec{ sampleRate, static_cast<juce::uint32>(juce::jmax(1, maximumBlockSize)), 1 };
	highPass.prepare(spec);
	lowPass.prepare(spec);
	appliedHighPass = -1.0f;
	appliedLowPass = -1.0f;
	refractorySamples = static_cast<int>(0.05 * sampleRate);
	meanCoefficient = 1.0f - std::exp(-static_cast<float>(kHopSize / (0.5 * sampleRate)));
	fastAttackCoeff = 1.0f - std::exp(-static_cast<float>(1.0 / (0.0005 * sampleRate)));
	fastReleaseCoeff = 1.0f - std::exp(-static_cast<float>(1.0 / (0.01 * sampleRate)));
	slowCoeff = 1.0f - std::exp(-static_cast<float>(1.0 / (0.05 * sampleRate)));
	reset();
}

void TransientDetector::reset()
{
	highPass.reset();
	lowPass.reset();
	std::fill(fifo.begin(), fifo.end(), 0.0f);
	std::fill(previousMagnitudes.begin(), previousMagnitudes.end(), 0.0f);
	fifoWritePos = 0;
	samplesUntilHop = kHopSize;
	fifoFilled = false;
	previousFlux = 0.0f;
	fluxMean = 0.0f;
	fastEnv = 0.0f;
	slowEnv = 0.0f;
	refractoryCounter = 0;
	samplesSinceLastTrigger = kFftSize;
}

void TransientDetector::setSensitivity(float newSensitivity) noexcept
{
	sensitivity.store(juce::jlimit(0.0f, 1.0f, newSensitivity));
}

juce::uint8 TransientDetector::intensityToVelocity(float intensity, float sensitivity) noexcept
{
	const auto dynamicVelocity = 1.0f + 126.0f * juce::jlimit(0.0f, 1.0f, intensity);
	return static_cast<juce::uint8>(juce::jlimit(1, 127,
		juce::roundToInt(juce::jmap(juce::jlimit(0.0f, 1.0f, sensitivity), 127.0f, dynamicVelocity))));
}

bool TransientDetector::processEnvelopeSample(float sample, float currentSensitivity) noexcept
{
	const auto amplitude = std::abs(sample);
	fastEnv += (amplitude > fastEnv ? fastAttackCoeff : fastReleaseCoeff) * (amplitude - fastEnv);
	slowEnv += slowCoeff * (amplitude - slowEnv);
	const auto ratio = juce::jmap(currentSensitivity, 6.0f, 1.5f);
	const auto gate = juce::jmap(currentSensitivity, 0.05f, 0.002f);
	return fastEnv > gate && fastEnv > slowEnv * ratio;
}

bool TransientDetector::analyseHop(float& intensityOut)
{
	// Copy the FIFO in chronological order and window it.
	for (int i = 0; i < kFftSize; ++i)
		fftData[static_cast<size_t>(i)] = fifo[static_cast<size_t>((fifoWritePos + i) % kFftSize)];
	std::fill(fftData.begin() + kFftSize, fftData.end(), 0.0f);

	window.multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(kFftSize));
	fft.performFrequencyOnlyForwardTransform(fftData.data());

	const int numBins = kFftSize / 2 + 1;
	float flux = 0.0f;
	for (int bin = 0; bin < numBins; ++bin)
	{
		const auto magnitude = std::log1p(kCompression * fftData[static_cast<size_t>(bin)] / static_cast<float>(kFftSize));
		auto& previous = previousMagnitudes[static_cast<size_t>(bin)];
		flux += std::max(0.0f, magnitude - previous);
		previous = magnitude;
	}

	const auto currentSensitivity = sensitivity.load();
	const auto multiplier = juce::jmap(currentSensitivity, 3.0f, 1.15f);
	const auto floor = juce::jmap(currentSensitivity, 1.0f, 0.05f);

	const bool detected = fifoFilled
		&& refractoryCounter <= 0
		&& flux > previousFlux
		&& flux > fluxMean * multiplier + floor;

	fluxMean += meanCoefficient * (flux - fluxMean);
	previousFlux = flux;
	intensityOut = detected ? juce::jlimit(0.0f, 1.0f, flux / kFluxFullScale) : 0.0f;
	return detected;
}

void TransientDetector::process(const juce::AudioBuffer<float>& input)
{
	const int numSamples = input.getNumSamples();
	const int numChannels = input.getNumChannels();
	if (numChannels == 0)
		return;

	const auto currentMode = mode.load();
	const auto currentSensitivity = sensitivity.load();
	const auto currentVelocitySensitivity = velocitySensitivity.load();
	const float channelGain = 1.0f / static_cast<float>(numChannels);

	const auto maxCutoff = static_cast<float>(0.49 * sampleRate);
	const auto targetHighPass = juce::jlimit(1.0f, maxCutoff, highPassFrequency.load());
	const auto targetLowPass = juce::jlimit(1.0f, maxCutoff, lowPassFrequency.load());
	if (targetHighPass != appliedHighPass)
	{
		highPass.setCutoffFrequency(targetHighPass);
		appliedHighPass = targetHighPass;
	}
	if (targetLowPass != appliedLowPass)
	{
		lowPass.setCutoffFrequency(targetLowPass);
		appliedLowPass = targetLowPass;
	}

	for (int i = 0; i < numSamples; ++i)
	{
		if (refractoryCounter > 0)
			--refractoryCounter;
		if (samplesSinceLastTrigger < kFftSize)
			++samplesSinceLastTrigger;

		float mono = 0.0f;
		for (int channel = 0; channel < numChannels; ++channel)
			mono += input.getReadPointer(channel)[i];

		const auto filtered = lowPass.processSample(0, highPass.processSample(0, mono * channelGain));
		const bool envelopeDetected = processEnvelopeSample(filtered, currentSensitivity);
		if (currentMode != DetectionMode::Spectral && envelopeDetected && refractoryCounter <= 0)
		{
			const auto levelDb = juce::Decibels::gainToDecibels(std::max(fastEnv, 1.0e-6f));
			const auto envelopeIntensity = juce::jlimit(0.0f, 1.0f,
				juce::jmap(levelDb, kIntensityFloorDb, 0.0f, 0.0f, 1.0f));
			player->trigger(i, intensityToVelocity(envelopeIntensity, currentVelocitySensitivity));
			transientCount.fetch_add(1);
			refractoryCounter = refractorySamples;
			samplesSinceLastTrigger = 0;
		}

		fifo[static_cast<size_t>(fifoWritePos)] = filtered;
		if (++fifoWritePos >= kFftSize)
		{
			fifoWritePos = 0;
			fifoFilled = true;
		}

		if (--samplesUntilHop > 0)
			continue;

		samplesUntilHop = kHopSize;
		float spectralIntensity = 0.0f;
		const bool spectralDetected = analyseHop(spectralIntensity);
		if (!spectralDetected || currentMode == DetectionMode::Envelope || refractoryCounter > 0
			|| (currentMode == DetectionMode::Hybrid && samplesSinceLastTrigger < kFftSize))
			continue;

		player->trigger(i, intensityToVelocity(spectralIntensity, currentVelocitySensitivity));
		transientCount.fetch_add(1);
		refractoryCounter = refractorySamples;
		samplesSinceLastTrigger = 0;
	}
}

} // namespace instrument
