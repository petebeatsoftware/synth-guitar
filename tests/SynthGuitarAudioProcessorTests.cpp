#include "SynthGuitarAudioProcessor.h"
#include "TestHelpers.h"

#include <cmath>
#include <set>

namespace
{
using StrumMode = SynthGuitarAudioProcessor::StrumMode;

struct ParameterSpec
{
	const char* id;
	float initial;
	float minimum;
	float maximum;
};

// (name, default, min, max) for all parameters in the plugin. Used to verify that the plugin's parameters are correctly defined.
constexpr ParameterSpec parameterSpecs[] {
	{ "detectionMode", 2.0f, 0.0f, 2.0f },
	{ SynthGuitarAudioProcessor::strumParameterId, static_cast<float>(StrumMode::Both),
		static_cast<float>(StrumMode::Low), static_cast<float>(StrumMode::Both) },
	{ "singleSensitivity", 0.5f, 0.0f, 1.0f },
	{ "singleLowFrequency", 20.0f, 20.0f, 20000.0f },
	{ "singleHighFrequency", 20000.0f, 20.0f, 20000.0f },
	{ "velocitySensitivity", 50.0f, 0.0f, 100.0f }
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
		check("Strum choice labels and indices remain compatible", [&]
		{
			expectEquals(static_cast<int>(StrumMode::Low), 0);
			expectEquals(static_cast<int>(StrumMode::High), 1);
			expectEquals(static_cast<int>(StrumMode::Both), 2);
			SynthGuitarAudioProcessor processor;
			auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(
				processor.getParameters().getParameter(SynthGuitarAudioProcessor::strumParameterId));
			expect(parameter != nullptr);
			if (parameter == nullptr)
				return;
			expect(parameter->choices == juce::StringArray{ "Low", "High", "Both" });
			for (const auto mode : { StrumMode::Low, StrumMode::High, StrumMode::Both })
			{
				test::setParameter(processor.getParameters(), SynthGuitarAudioProcessor::strumParameterId,
					static_cast<float>(mode));
				expectEquals(parameter->getIndex(), static_cast<int>(mode));
			}
		});
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
		check("Velocity sensitivity reaches the single-band detector", [&]
		{
			for (const float sensitivity : { 0.0f, 100.0f })
			{
				SynthGuitarAudioProcessor processor;
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
					{
						SynthGuitarAudioProcessor processor;
						auto layout = processor.getBusesLayout();
						const auto channelSet = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
						layout.inputBuses.set(0, channelSet);
						layout.outputBuses.set(0, channelSet);
						expect(processor.setBusesLayout(layout));
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
			expect(!processor.isSingleNoteActive());
			juce::AudioBuffer<float> empty(2, 0);
			processor.processBlock(empty, midi);
			expect(midi.isEmpty());
		});
		for (const auto& modes : { std::pair{ StrumMode::Low, StrumMode::High },
			std::pair{ StrumMode::Low, StrumMode::Both }, std::pair{ StrumMode::High, StrumMode::Low },
			std::pair{ StrumMode::High, StrumMode::Both }, std::pair{ StrumMode::Both, StrumMode::Low },
			std::pair{ StrumMode::Both, StrumMode::High } })
			check("Strum routing and held-note replay from "
				+ juce::String(SynthGuitarAudioProcessor::strumModeNames[static_cast<size_t>(modes.first)])
				+ " to " + SynthGuitarAudioProcessor::strumModeNames[static_cast<size_t>(modes.second)], [&]
			{
				const auto strum = modes.first;
				const auto nextStrum = modes.second;
				SynthGuitarAudioProcessor processor;
				test::setParameter(processor.getParameters(), SynthGuitarAudioProcessor::strumParameterId, static_cast<float>(strum));
				test::setParameter(processor.getParameters(), "detectionMode", 1.0f);
				processor.prepareToPlay(48000.0, 512);
				holdChord(processor);
				const auto events = test::eventsOf(excite(processor));
				expectEquals(static_cast<int>(events.size()), strum == StrumMode::Both ? 2 : 1);
				for (size_t i = 0; i < events.size(); ++i)
				{
					expect(events[i].second.isNoteOn());
					expectEquals(events[i].second.getNoteNumber(), strum == StrumMode::High || i == 1 ? 60 : 48);
					expectEquals(events[i].second.getChannel(), 1);
				}
				expect(processor.isSingleNoteActive());
				test::setParameter(processor.getParameters(), SynthGuitarAudioProcessor::strumParameterId, static_cast<float>(nextStrum));
				juce::AudioBuffer<float> silence(2, 512);
				silence.clear();
				juce::MidiBuffer midi;
				processor.processBlock(silence, midi);
				const auto released = test::eventsOf(midi);
				expectEquals(static_cast<int>(released.size()), strum == StrumMode::Both ? 2 : 1);
				for (size_t i = 0; i < released.size(); ++i)
					expectEvent(released, i, 0, strum == StrumMode::High || i == 1 ? 60 : 48, false);
				expect(!processor.isSingleNoteActive());
				for (int block = 0; block < 5; ++block)
				{
					midi.clear();
					processor.processBlock(silence, midi);
				}
				const auto retriggered = test::eventsOf(excite(processor));
				expectEquals(static_cast<int>(retriggered.size()), nextStrum == StrumMode::Both ? 2 : 1);
				for (size_t i = 0; i < retriggered.size(); ++i)
				{
					expect(retriggered[i].second.isNoteOn());
					expectEquals(retriggered[i].second.getNoteNumber(), nextStrum == StrumMode::High || i == 1 ? 60 : 48);
					expectEquals(retriggered[i].second.getChannel(), 1);
				}
			});
		check("Both strum keeps displaced low notes playing and releases only requested notes", [&]
		{
			SynthGuitarAudioProcessor processor;
			test::setParameter(processor.getParameters(), SynthGuitarAudioProcessor::strumParameterId,
				static_cast<float>(StrumMode::Both));
			test::setParameter(processor.getParameters(), "detectionMode", 1.0f);
			processor.prepareToPlay(48000.0, 512);
			juce::AudioBuffer<float> silence(2, 512);
			silence.clear();
			auto midi = test::buffer({ { test::on(48), 0 }, { test::on(60), 1 }, { test::on(72), 2 } });
			processor.processBlock(silence, midi);
			const auto triggered = test::eventsOf(excite(processor));
			expectEquals(static_cast<int>(triggered.size()), 3);
			for (size_t i = 0; i < triggered.size(); ++i)
			{
				expect(triggered[i].second.isNoteOn());
				expectEquals(triggered[i].second.getNoteNumber(), 48 + static_cast<int>(i) * 12);
			}
			midi = test::buffer({ { test::on(36), 5 }, { test::off(72), 7 } });
			processor.processBlock(silence, midi);
			const auto released = test::eventsOf(midi);
			expectEquals(static_cast<int>(released.size()), 1);
			expectEvent(released, 0, 7, 72, false);
			expect(processor.isSingleNoteActive(), "Displaced low note and upper note remain playing");
			midi = test::buffer({ { test::off(36), 3 }, { test::off(48), 5 }, { test::off(60), 7 } });
			processor.processBlock(silence, midi);
			const auto remaining = test::eventsOf(midi);
			expectEquals(static_cast<int>(remaining.size()), 2);
			expectEvent(remaining, 0, 5, 48, false);
			expectEvent(remaining, 1, 7, 60, false);
			expect(!processor.isSingleNoteActive());
			for (int block = 0; block < 5; ++block)
			{
				midi.clear();
				processor.processBlock(silence, midi);
			}
			expect(excite(processor).isEmpty(), "Released notes are not retriggered");
		});
		check("Release and reprepare clear playing and held notes", [&]
		{
			SynthGuitarAudioProcessor processor;
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
