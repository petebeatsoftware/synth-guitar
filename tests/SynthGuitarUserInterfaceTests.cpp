#include "SynthGuitarAudioProcessor.h"
#include "RenderedUserInterface.h"
#include "TestHelpers.h"

namespace
{
class RenderedUserInterfaceTests final : public test::Suite
{
public:
	RenderedUserInterfaceTests() : Suite("RenderedUserInterface") {}

	void runTest() override
	{
		check("Factory creates the expected resizable editor", [&]
		{
			SynthGuitarAudioProcessor processor;
			std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
			expect(dynamic_cast<RenderedUserInterface*>(editor.get()) != nullptr);
			expect(editor->getAudioProcessor() == &processor);
			expectEquals(editor->getWidth(), 600);
			expectEquals(editor->getHeight(), 810);
			expect(editor->isResizable());
		});
		check("Rendered control loads, attaches and scales with its canvas", [&]
		{
			const auto currentLayout = juce::JSON::parse(
				juce::File(SYNTHGUITAR_RENDERED_UI_DIR).getChildFile("juce_layout.json"));
			if (currentLayout["Camera_LowFrequency"].isVoid())
				return;
			SynthGuitarAudioProcessor processor;
			RenderedUserInterface editor(processor);
			juce::Component* canvas = nullptr;
			FilmstripKnob* knob = nullptr;
			for (auto* child : editor.getChildren())
				for (auto* control : child->getChildren())
					if (auto* candidate = dynamic_cast<FilmstripKnob*>(control))
						if (candidate->getName() == "Camera_LowFrequency")
						{
							canvas = child;
							knob = candidate;
						}
			expect(knob != nullptr, "Provided JSON and filmstrip load successfully");
			if (knob == nullptr)
				return;
			const auto layout = juce::JSON::parse(
				juce::File(SYNTHGUITAR_RENDERED_UI_DIR).getChildFile("juce_layout.json"));
			const auto entry = layout["Camera_LowFrequency"];
			const juce::Rectangle<int> expectedBounds(static_cast<int>(entry["x"]), static_cast<int>(entry["y"]),
				static_cast<int>(entry["width"]), static_cast<int>(entry["height"]));
			expect(knob->getBounds() == expectedBounds);
			expect(knob->getSliderStyle() == juce::Slider::RotaryVerticalDrag);
			expect(knob->getTextBoxPosition() == juce::Slider::NoTextBox);
			knob->setValue(200.0, juce::sendNotificationSync);
			expectWithinAbsoluteError(processor.getParameters().getRawParameterValue("singleLowFrequency")->load(),
								  200.0f, 0.01f);
			test::setParameter(processor.getParameters(), "singleLowFrequency", 300.0f);
			expectWithinAbsoluteError(knob->getValue(), 300.0, 0.01);
			for (const auto size : { juce::Point<int>(400, 540), juce::Point<int>(800, 1080) })
			{
				editor.setSize(size.x, size.y);
				expect(canvas->getBoundsInParent() == editor.getLocalBounds());
				expect(knob->getBounds() == expectedBounds);
				juce::Image image(juce::Image::ARGB, size.x, size.y, true);
				{
					juce::Graphics graphics(image);
					editor.paintEntireComponent(graphics, true);
				}
			}
		});
		check("Sensitivity and high frequency filmstrips attach in both directions", [&]
		{
			SynthGuitarAudioProcessor processor;
			RenderedUserInterface editor(processor);
			const auto layout = juce::JSON::parse(
				juce::File(SYNTHGUITAR_RENDERED_UI_DIR).getChildFile("juce_layout.json"));
			for (const auto& mapping : { std::pair<const char*, const char*>{ "Camera_Sensitivity", "singleSensitivity" },
				std::pair<const char*, const char*>{ "Camera_HighFrequency", "singleHighFrequency" } })
			{
				FilmstripKnob* knob = nullptr;
				for (auto* canvas : editor.getChildren())
					for (auto* child : canvas->getChildren())
						if (child->getName() == mapping.first)
							knob = dynamic_cast<FilmstripKnob*>(child);
				expect(knob != nullptr, juce::String(mapping.first) + " loads as a rotary knob");
				if (knob == nullptr)
					continue;
				const auto entry = layout[mapping.first];
				expect(knob->getBounds() == juce::Rectangle<int>(static_cast<int>(entry["x"]), static_cast<int>(entry["y"]),
					static_cast<int>(entry["width"]), static_cast<int>(entry["height"])));
				expect(knob->getSliderStyle() == juce::Slider::RotaryVerticalDrag);
				const auto value = juce::String(mapping.second) == "singleSensitivity" ? 0.75 : 5000.0;
				knob->setValue(value, juce::sendNotificationSync);
				expectWithinAbsoluteError(processor.getParameters().getRawParameterValue(mapping.second)->load(),
					static_cast<float>(value), 0.001f);
				test::setParameter(processor.getParameters(), mapping.second, static_cast<float>(value * 0.5));
				expectWithinAbsoluteError(knob->getValue(), value * 0.5, 0.001);
			}
		});
		check("New rendered controls load, cycle choices and display MIDI and transient activity", [&]
		{
			SynthGuitarAudioProcessor processor;
			processor.prepareToPlay(48000.0, 512);
			RenderedUserInterface editor(processor);
			auto findControl = [&](const juce::String& controlName) -> FilmstripControl*
			{
				for (auto* canvas : editor.getChildren())
					for (auto* child : canvas->getChildren())
						if (child->getName() == controlName)
							return dynamic_cast<FilmstripControl*>(child);
				return nullptr;
			};
			auto* detect = findControl("Camera_Detect");
			auto* midiIn = findControl("Camera_MidiIn");
			auto* midiOut = findControl("Camera_MidiOut");
			auto* strings = findControl("Camera_Strings");
			auto* plectrums = findControl("Camera_Plectrums");
			auto* detection = findControl("Camera_DetectionMode");
			expect(detect && midiIn && midiOut && strings && plectrums && detection);
			if (!(detect && midiIn && midiOut && strings && plectrums && detection))
				return;
			expect(plectrums->getX() < 0 && strings->getY() < 0, "Off-canvas crops still load");
			for (int state = 0; state < 2; ++state)
			{
				test::setParameter(processor.getParameters(), "strum", static_cast<float>(state));
				expectEquals(plectrums->getFrame(), state);
				plectrums->keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
				expectEquals(processor.getParameters().getRawParameterValue("strum")->load(), static_cast<float>(1 - state));
				expectEquals(plectrums->getFrame(), 1 - state);
			}
			for (int state = 0; state < 3; ++state)
			{
				test::setParameter(processor.getParameters(), "detectionMode", static_cast<float>(state));
				expectEquals(detection->getFrame(), state);
				detection->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
				expectEquals(detection->getFrame(), (state + 1) % 3);
				expectEquals(processor.getParameters().getRawParameterValue("detectionMode")->load(), static_cast<float>((state + 1) % 3));
			}
			auto waitForFrame = [](FilmstripControl& control, int frame)
			{
				const auto deadline = juce::Time::getMillisecondCounterHiRes() + 2000.0;
				while (control.getFrame() != frame && juce::Time::getMillisecondCounterHiRes() < deadline)
				{
					juce::Thread::sleep(5);
					juce::Timer::callPendingTimersSynchronously();
				}
			};
			juce::AudioBuffer<float> audio(2, 512);
			audio.clear();
			juce::MidiBuffer midi;
			test::setParameter(processor.getParameters(), "detectionMode", 1.0f);
			juce::Random random(1234);
			for (int i = 0; i < 512; ++i)
			{
				const auto value = i < 480 ? random.nextFloat() * 1.6f - 0.8f : 0.0f;
				for (int channel = 0; channel < 2; ++channel)
					audio.setSample(channel, i, value);
			}
			const auto before = processor.getTransientCount();
			processor.processBlock(audio, midi);
			expect(processor.getTransientCount() > before, "Audio burst creates a transient");
			waitForFrame(*detect, 1);
			expectEquals(detect->getFrame(), 1, "Transient flashes without held MIDI notes");
			expectEquals(midiOut->getFrame(), 0);
			expectEquals(strings->getFrame(), 0);
			waitForFrame(*detect, 0);
			expectEquals(detect->getFrame(), 0, "Flash expires independently of MIDI");
			processor.releaseResources();
			processor.prepareToPlay(48000.0, 512);
			test::setParameter(processor.getParameters(), "detectionMode", 2.0f);
			audio.clear();
			midi.clear();
			midi.addEvent(test::on(48), 0);
			midi.addEvent(test::on(60), 0);
			processor.processBlock(audio, midi);
			waitForFrame(*midiIn, 1);
			expectEquals(midiIn->getFrame(), 1);
			juce::Random outputRandom(1234);
			for (int i = 0; i < 512; ++i)
			{
				const auto value = i < 480 ? outputRandom.nextFloat() * 1.6f - 0.8f : 0.0f;
				for (int channel = 0; channel < 2; ++channel)
					audio.setSample(channel, i, value);
			}
			midi.clear();
			processor.processBlock(audio, midi);
			expect(processor.isOutputNoteActive(), "Second burst triggers the held MIDI note");
			waitForFrame(*midiOut, 1);
			waitForFrame(*strings, 3);
			expectEquals(midiOut->getFrame(), 1);
			expectEquals(strings->getFrame(), 3);
			processor.releaseResources();
			waitForFrame(*midiIn, 0);
			waitForFrame(*midiOut, 0);
			waitForFrame(*strings, 0);
			expectEquals(midiIn->getFrame(), 0);
			expectEquals(midiOut->getFrame(), 0);
			expectEquals(strings->getFrame(), 0);
		});
		check("Filmstrip selects first, middle and last frames using normalized slider values", [&]
		{
			juce::Image strip(juce::Image::ARGB, 2, 6, true);
			const juce::Colour colours[] = { juce::Colours::red, juce::Colours::green, juce::Colours::blue };
			{
				juce::Graphics graphics(strip);
				for (int frame = 0; frame < 3; ++frame)
				{
					graphics.setColour(colours[frame]);
					graphics.fillRect(0, frame * 2, 2, 2);
				}
			}
			FilmstripKnob knob(strip, 3);
			knob.setSize(2, 2);
			knob.setNormalisableRange(juce::NormalisableRange<double>(100.0, 1000.0, 0.0, 0.5));
			for (int frame = 0; frame < 3; ++frame)
			{
				knob.setValue(knob.proportionOfLengthToValue(static_cast<double>(frame) / 2.0),
							  juce::dontSendNotification);
				juce::Image image(juce::Image::ARGB, 2, 2, true);
				{
					juce::Graphics graphics(image);
					knob.paint(graphics);
				}
				expectEquals(image.getPixelAt(1, 1).toString(), colours[frame].toString());
			}
		});
	}
};

static RenderedUserInterfaceTests renderedUserInterfaceTests;
}
