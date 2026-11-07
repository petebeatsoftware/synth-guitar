#include "SynthGuitarAudioProcessor.h"
#include "TestHelpers.h"

#include <cmath>
#include <set>

namespace
{
struct ParameterSpec
{
	const char* id;
	float initial;
	float minimum;
	float maximum;
};

constexpr ParameterSpec parameterSpecs[] {
	{ "sensitivityLow", 0.5f, 0.0f, 1.0f },
	{ "sensitivityMid", 0.5f, 0.0f, 1.0f },
	{ "sensitivityHigh", 0.5f, 0.0f, 1.0f },
	{ "detectionMode", 2.0f, 0.0f, 2.0f },
	{ "bandMode", 1.0f, 0.0f, 1.0f },
	{ "strum", 1.0f, 0.0f, 1.0f },
	{ "singleSensitivity", 0.5f, 0.0f, 1.0f },
	{ "lowToMid", 200.0f, 20.0f, 20000.0f },
	{ "midToHigh", 1000.0f, 20.0f, 20000.0f },
	{ "singleLowFrequency", 20.0f, 20.0f, 20000.0f },
	{ "singleHighFrequency", 20000.0f, 20.0f, 20000.0f },
	{ "velocitySensitivity", 0.0f, 0.0f, 100.0f }
};

juce::MidiBuffer excite(SynthGuitarAudioProcessor& processor)
{
	juce::AudioBuffer<float> audio(2, 512);
	juce::Random random(1234);
	for (int i = 0; i < 512; ++i)
	{
		const auto value = i < 480 ? random.nextFloat() * 1.6f - 0.8f : 0.0f;
		for (int channel = 0; channel < 2; ++channel)
			audio.setSample(channel, i, value);
	}
	juce::MidiBuffer output;
	processor.processBlock(audio, output);
	return output;
}

void holdChord(SynthGuitarAudioProcessor& processor)
{
	juce::AudioBuffer<float> audio(2, 512);
	audio.clear();
	auto midi = test::buffer({ { test::on(48), 0 }, { test::on(60), 1 } });
	processor.processBlock(audio, midi);
}

class SynthGuitarAudioProcessorTests final : public test::Suite
{
public:
	SynthGuitarAudioProcessorTests() : Suite("SynthGuitarAudioProcessor") {}

	void runTest() override
	{
		check("MIDI activity tracks channels, note-offs and lifecycle resets", [&]
		{
			SynthGuitarAudioProcessor processor;
			processor.prepareToPlay(48000.0, 512);
			juce::AudioBuffer<float> audio(2, 512);
			audio.clear();
			juce::MidiBuffer midi;
			midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
			midi.addEvent(juce::MidiMessage::noteOn(2, 60, (juce::uint8) 100), 1);
			processor.processBlock(audio, midi);
			expect(processor.isInputNoteActive());
			expect(!processor.isOutputNoteActive());
			midi.clear();
			midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
			processor.processBlock(audio, midi);
			expect(processor.isInputNoteActive(), "Other channel remains held");
			midi.clear();
			midi.addEvent(juce::MidiMessage::allNotesOff(2), 0);
			processor.processBlock(audio, midi);
			expect(!processor.isInputNoteActive());
			holdChord(processor);
			expect(processor.isInputNoteActive());
			excite(processor);
			expect(processor.isOutputNoteActive());
			processor.releaseResources();
			expect(!processor.isInputNoteActive() && !processor.isOutputNoteActive());
			processor.prepareToPlay(48000.0, 512);
			const auto before = processor.getTransientCount();
			excite(processor);
			expect(processor.getTransientCount() > before, "Transients counted without held notes");
			expect(!processor.isOutputNoteActive());
		});
		for (const int mode : { 0, 1 })
			check("Velocity sensitivity reaches detectors in band mode " + juce::String(mode), [&]
			{
				for (const float sensitivity : { 0.0f, 100.0f })
				{
					SynthGuitarAudioProcessor processor;
					test::setParameter(processor.getParameters(), "bandMode", static_cast<float>(mode));
					test::setParameter(processor.getParameters(), "velocitySensitivity", sensitivity);
					processor.prepareToPlay(48000.0, 512);
					holdChord(processor);
					const auto output = excite(processor);
					bool noteOn = false;
					bool dynamic = false;
					for (const auto metadata : output)
					{
						const auto message = metadata.getMessage();
						if (message.isNoteOn())
						{
							noteOn = true;
							dynamic = dynamic || message.getVelocity() < 127;
							if (sensitivity == 0.0f)
								expectEquals(static_cast<int>(message.getVelocity()), 127);
						}
					}
					expect(noteOn);
					if (sensitivity == 100.0f)
						expect(dynamic);
				}
			});
		check("Plugin metadata and single-program API", [&]
		{
			SynthGuitarAudioProcessor processor;
			expectEquals(processor.getName(), juce::String("Instrument"));
			expect(processor.acceptsMidi());
			expect(processor.producesMidi());
			expect(!processor.isMidiEffect());
			expect(processor.hasEditor());
			expectEquals(processor.getTailLengthSeconds(), 0.0);
			expectEquals(processor.getLatencySamples(), 0);
			expectEquals(processor.getNumPrograms(), 1);
			processor.setCurrentProgram(0);
			processor.changeProgramName(0, "Ignored");
			expectEquals(processor.getCurrentProgram(), 0);
			expect(processor.getProgramName(0).isEmpty());
		});
		check("Only matched mono and stereo buses are supported", [&]
		{
			SynthGuitarAudioProcessor processor;
			for (const auto& input : { juce::AudioChannelSet::disabled(), juce::AudioChannelSet::mono(),
									  juce::AudioChannelSet::stereo(), juce::AudioChannelSet::create5point1() })
				for (const auto& output : { juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo(),
										   juce::AudioChannelSet::create5point1() })
				{
					auto layout = processor.getBusesLayout();
					layout.inputBuses.set(0, input);
					layout.outputBuses.set(0, output);
					const bool supported = input == output && (input == juce::AudioChannelSet::mono()
																|| input == juce::AudioChannelSet::stereo());
					expect(processor.isBusesLayoutSupported(layout) == supported);
				}
		});
		for (const auto& spec : parameterSpecs)
			check(juce::String(spec.id) + " default range and normalized conversion", [&]
			{
				SynthGuitarAudioProcessor processor;
				auto& state = processor.getParameters();
				auto* parameter = state.getParameter(spec.id);
				expect(parameter != nullptr);
				if (parameter == nullptr)
					return;
				expectWithinAbsoluteError(state.getRawParameterValue(spec.id)->load(), spec.initial, 0.001f);
				expectEquals(parameter->convertFrom0to1(0.0f), spec.minimum);
				expectEquals(parameter->convertFrom0to1(1.0f), spec.maximum);
				expectWithinAbsoluteError(parameter->convertFrom0to1(parameter->convertTo0to1(spec.initial)),
										  spec.initial, 0.01f);
			});
		check("Parameter IDs are unique and complete", [&]
		{
			SynthGuitarAudioProcessor processor;
			std::set<juce::String> ids;
			for (auto* parameter : processor.juce::AudioProcessor::getParameters())
				if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
					ids.insert(identified->paramID);
			expectEquals(static_cast<int>(ids.size()), static_cast<int>(std::size(parameterSpecs)));
		});
		check("State round trip restores every parameter", [&]
		{
			SynthGuitarAudioProcessor source;
			SynthGuitarAudioProcessor destination;
			for (const auto& spec : parameterSpecs)
				test::setParameter(source.getParameters(), spec.id,
								   spec.initial == spec.maximum ? spec.minimum : spec.maximum);
			juce::MemoryBlock state;
			source.getStateInformation(state);
			expect(state.getSize() > 0);
			destination.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
			for (const auto& spec : parameterSpecs)
				expectEquals(destination.getParameters().getRawParameterValue(spec.id)->load(),
							 source.getParameters().getRawParameterValue(spec.id)->load(), spec.id);
		});
		check("Empty corrupt and wrong-root state leave parameters unchanged", [&]
		{
			SynthGuitarAudioProcessor processor;
			juce::MemoryBlock before;
			processor.getStateInformation(before);
			processor.setStateInformation(before.getData(), 0);
			const char corrupt[] = "not plugin state";
			processor.setStateInformation(corrupt, sizeof(corrupt));
			juce::MemoryBlock wrongRoot;
			juce::XmlElement xml("WRONG_ROOT");
			juce::AudioProcessor::copyXmlToBinary(xml, wrongRoot);
			processor.setStateInformation(wrongRoot.getData(), static_cast<int>(wrongRoot.getSize()));
			juce::MemoryBlock after;
			processor.getStateInformation(after);
			expect(before == after);
		});
		for (const auto rate : { 44100.0, 48000.0, 96000.0 })
			for (const int blockSize : { 1, 128, 512 })
				check("Silent output " + juce::String(rate) + " Hz / " + juce::String(blockSize) + " samples", [&]
				{
					for (const int channels : { 1, 2 })
						for (const int bandMode : { 0, 1 })
						{
							SynthGuitarAudioProcessor processor;
							auto layout = processor.getBusesLayout();
							const auto channelSet = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
							layout.inputBuses.set(0, channelSet);
							layout.outputBuses.set(0, channelSet);
							expect(processor.setBusesLayout(layout));
							test::setParameter(processor.getParameters(), processor.bandModeParameterId, static_cast<float>(bandMode));
							processor.prepareToPlay(rate, blockSize);
							juce::AudioBuffer<float> audio(channels, blockSize);
							for (int channel = 0; channel < channels; ++channel)
								for (int sample = 0; sample < blockSize; ++sample)
									audio.setSample(channel, sample, std::sin(static_cast<float>((sample + channel * 17) * 0.13)));
							juce::MidiBuffer midi;
							processor.processBlock(audio, midi);
							expectEquals(audio.getNumChannels(), channels);
							expectEquals(processor.getTotalNumOutputChannels(), channels);
							for (int channel = 0; channel < channels; ++channel)
								for (int sample = 0; sample < blockSize; ++sample)
									expectEquals(audio.getSample(channel, sample), 0.0f);
						}
				});
		check("Silent input consumes held MIDI without generating notes; zero samples are safe", [&]
		{
			SynthGuitarAudioProcessor processor;
			processor.prepareToPlay(48000.0, 512);
			juce::AudioBuffer<float> audio(2, 512);
			audio.clear();
			auto midi = test::buffer({ { test::on(60), 0 } });
			processor.processBlock(audio, midi);
			expect(midi.isEmpty());
			for (int band = 0; band < processor.numBands; ++band)
				expect(!processor.isTransientNoteActive(band));
			expect(!processor.isSingleNoteActive());
			juce::AudioBuffer<float> empty(2, 0);
			processor.processBlock(empty, midi);
			expect(midi.isEmpty());
		});
		for (const int strum : { 0, 1 })
			check(strum == 0 ? "Single low strum plays lowest note" : "Single high strum plays upper note", [&]
			{
				SynthGuitarAudioProcessor processor;
				test::setParameter(processor.getParameters(), "bandMode", 0.0f);
				test::setParameter(processor.getParameters(), "strum", static_cast<float>(strum));
				test::setParameter(processor.getParameters(), "detectionMode", 1.0f);
				processor.prepareToPlay(48000.0, 512);
				holdChord(processor);
				const auto events = test::eventsOf(excite(processor));
				expectEquals(static_cast<int>(events.size()), 1);
				if (!events.empty())
				{
					expect(events[0].second.isNoteOn());
					expectEquals(events[0].second.getNoteNumber(), strum == 0 ? 48 : 60);
					expectEquals(events[0].second.getChannel(), 1);
				}
				expect(processor.isSingleNoteActive());
				test::setParameter(processor.getParameters(), "bandMode", 1.0f);
				juce::AudioBuffer<float> silence(2, 512);
				silence.clear();
				juce::MidiBuffer midi;
				processor.processBlock(silence, midi);
				expectEvent(test::eventsOf(midi), 0, 0, strum == 0 ? 48 : 60, false);
				expect(!processor.isSingleNoteActive());
			});
		check("Multi-band triggers route notes to each band's channel and release on mode change", [&]
		{
			SynthGuitarAudioProcessor processor;
			test::setParameter(processor.getParameters(), "detectionMode", 1.0f);
			for (const auto* id : processor.sensitivityParameterIds)
				test::setParameter(processor.getParameters(), id, 1.0f);
			processor.prepareToPlay(48000.0, 512);
			holdChord(processor);
			const auto events = test::eventsOf(excite(processor));
			for (int band = 0; band < processor.numBands; ++band)
			{
				bool found = false;
				for (const auto& event : events)
					found = found || (event.second.isNoteOn() && event.second.getChannel() == band + 1
									  && event.second.getNoteNumber() == (band == 0 ? 48 : 60));
				expect(found, "Band " + juce::String(band) + " emits expected note");
				expect(processor.isTransientNoteActive(band));
			}
			test::setParameter(processor.getParameters(), "bandMode", 0.0f);
			juce::AudioBuffer<float> silence(2, 512);
			silence.clear();
			juce::MidiBuffer midi;
			processor.processBlock(silence, midi);
			expectEquals(midi.getNumEvents(), 3);
			for (int band = 0; band < processor.numBands; ++band)
			{
				expectEvent(test::eventsOf(midi), static_cast<size_t>(band), 0, band == 0 ? 48 : 60, false, band + 1);
				expect(!processor.isTransientNoteActive(band));
			}
		});
		check("Release and reprepare clear playing and held notes", [&]
		{
			SynthGuitarAudioProcessor processor;
			test::setParameter(processor.getParameters(), "bandMode", 0.0f);
			processor.prepareToPlay(48000.0, 512);
			holdChord(processor);
			excite(processor);
			expect(processor.isSingleNoteActive());
			processor.releaseResources();
			expect(!processor.isSingleNoteActive());
			processor.prepareToPlay(44100.0, 512);
			expect(excite(processor).isEmpty());
		});
	}
};

static SynthGuitarAudioProcessorTests synthGuitarAudioProcessorTests;
}
