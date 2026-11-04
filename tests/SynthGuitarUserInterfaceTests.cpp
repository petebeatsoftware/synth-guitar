#include "SynthGuitarAudioProcessor.h"
#include "SynthGuitarUserInterface.h"
#include "TestHelpers.h"

namespace
{
juce::ComboBox* findChoice(juce::Component& editor, const juce::String& firstItem)
{
	for (auto* child : editor.getChildren())
		if (auto* choice = dynamic_cast<juce::ComboBox*>(child))
			if (choice->getItemText(0) == firstItem)
				return choice;
	return nullptr;
}

std::vector<juce::Slider*> visibleSliders(juce::Component& editor)
{
	std::vector<juce::Slider*> sliders;
	for (auto* child : editor.getChildren())
		if (auto* slider = dynamic_cast<juce::Slider*>(child))
			if (slider->isVisible())
				sliders.push_back(slider);
	return sliders;
}

class SynthGuitarUserInterfaceTests final : public test::Suite
{
public:
	SynthGuitarUserInterfaceTests() : Suite("SynthGuitarUserInterface") {}

	void runTest() override
	{
		check("Factory creates the expected resizable editor", [&]
		{
			SynthGuitarAudioProcessor processor;
			std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
			expect(dynamic_cast<SynthGuitarUserInterface*>(editor.get()) != nullptr);
			expect(editor->getAudioProcessor() == &processor);
			expectEquals(editor->getWidth(), 480);
			expectEquals(editor->getHeight(), 420);
			expect(editor->isResizable());
		});
		for (const int mode : { 0, 1 })
			check(mode == 0 ? "Single mode shows four sliders and strum" : "Multi mode shows six sliders and hides strum", [&]
			{
				SynthGuitarAudioProcessor processor;
				test::setParameter(processor.getParameters(), "bandMode", static_cast<float>(mode));
				SynthGuitarUserInterface editor(processor);
				expectEquals(static_cast<int>(visibleSliders(editor).size()), mode == 0 ? 4 : 6);
				auto* strum = findChoice(editor, "Low");
				auto* bandMode = findChoice(editor, "Single");
				auto* detection = findChoice(editor, "Spectral");
				expect(strum != nullptr && bandMode != nullptr && detection != nullptr);
				if (strum == nullptr || bandMode == nullptr || detection == nullptr)
					return;
				expect(strum->isVisible() == (mode == 0));
				expectEquals(bandMode->getSelectedId(), mode + 1);
				expectEquals(detection->getSelectedId(), 3);
				for (const auto size : { juce::Point<int>(400, 400), juce::Point<int>(720, 560) })
				{
					editor.setSize(size.x, size.y);
					for (auto* slider : visibleSliders(editor))
					{
						expect(!slider->getBounds().isEmpty());
						expect(editor.getLocalBounds().contains(slider->getBounds()));
					}
					juce::Image image(juce::Image::ARGB, size.x, size.y, true);
					{
						juce::Graphics graphics(image);
						editor.paint(graphics);
					}
					expectEquals(image.getPixelAt(0, 0).toString(), juce::Colour(0xff171a21).toString(),
								 "Editor background colour after rendering completes");
					{
						juce::Graphics graphics(image);
						editor.paintEntireComponent(graphics, true);
					}
				}
			});
		check("Changing mode updates controls on an existing editor", [&]
		{
			SynthGuitarAudioProcessor processor;
			SynthGuitarUserInterface editor(processor);
			auto* bandMode = findChoice(editor, "Single");
			auto* strum = findChoice(editor, "Low");
			expect(bandMode != nullptr && strum != nullptr);
			if (bandMode == nullptr || strum == nullptr)
				return;
			for (const int mode : { 0, 1 })
			{
				if (mode == 0)
					bandMode->setSelectedId(1, juce::sendNotificationSync);
				else
					test::setParameter(processor.getParameters(), "bandMode", 1.0f);
				const int expectedSliders = mode == 0 ? 4 : 6;
				const auto deadline = juce::Time::getMillisecondCounterHiRes() + 2000.0;
				while (static_cast<int>(visibleSliders(editor).size()) != expectedSliders
					   && juce::Time::getMillisecondCounterHiRes() < deadline)
				{
					juce::Thread::sleep(5);
					juce::Timer::callPendingTimersSynchronously();
				}
				expectEquals(static_cast<int>(visibleSliders(editor).size()), expectedSliders,
							 "Timer updates mode visibility");
				expect(strum->isVisible() == (mode == 0));
				expectEquals(bandMode->getSelectedId(), mode + 1);
				expectEquals(processor.getParameters().getRawParameterValue("bandMode")->load(),
							 static_cast<float>(mode));
				for (auto* slider : visibleSliders(editor))
				{
					expect(!slider->getBounds().isEmpty());
					expect(editor.getLocalBounds().contains(slider->getBounds()));
				}
			}
		});
		check("Choice attachment propagates changes in both directions", [&]
		{
			SynthGuitarAudioProcessor processor;
			SynthGuitarUserInterface editor(processor);
			auto* detection = findChoice(editor, "Spectral");
			expect(detection != nullptr);
			if (detection == nullptr)
				return;
			detection->setSelectedId(2, juce::sendNotificationSync);
			expectEquals(processor.getParameters().getRawParameterValue("detectionMode")->load(), 1.0f);
			test::setParameter(processor.getParameters(), "detectionMode", 0.0f);
			expectEquals(detection->getSelectedId(), 1);
		});
		for (const int mode : { 0, 1 })
			check("Velocity attachment in mode " + juce::String(mode), [&]
			{
				SynthGuitarAudioProcessor processor;
				test::setParameter(processor.getParameters(), "bandMode", static_cast<float>(mode));
				SynthGuitarUserInterface editor(processor);
				juce::Slider* velocity = nullptr;
				for (auto* slider : visibleSliders(editor))
					if (slider->getMaximum() == 100.0)
						velocity = slider;
				expect(velocity != nullptr);
				if (velocity == nullptr)
					return;
				expectEquals(velocity->getValue(), 0.0);
				velocity->setValue(75.0, juce::sendNotificationSync);
				expectEquals(processor.getParameters().getRawParameterValue("velocitySensitivity")->load(), 75.0f);
				test::setParameter(processor.getParameters(), "velocitySensitivity", 25.0f);
				expectEquals(velocity->getValue(), 25.0);
			});
		check("Slider attachment propagates changes in both directions", [&]
		{
			SynthGuitarAudioProcessor processor;
			test::setParameter(processor.getParameters(), "bandMode", 0.0f);
			SynthGuitarUserInterface editor(processor);
			juce::Slider* sensitivity = nullptr;
			for (auto* slider : visibleSliders(editor))
				if (slider->getMaximum() == 1.0)
					sensitivity = slider;
			expect(sensitivity != nullptr);
			if (sensitivity == nullptr)
				return;
			sensitivity->setValue(0.75, juce::sendNotificationSync);
			expectWithinAbsoluteError(processor.getParameters().getRawParameterValue("singleSensitivity")->load(),
									  0.75f, 0.001f);
			test::setParameter(processor.getParameters(), "singleSensitivity", 0.25f);
			expectWithinAbsoluteError(sensitivity->getValue(), 0.25, 0.001);
		});
	}
};

static SynthGuitarUserInterfaceTests synthGuitarUserInterfaceTests;
}
