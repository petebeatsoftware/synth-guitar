#include "RenderedUserInterface.h"

#include "RenderedUIData.h"
#include "SynthGuitarAudioProcessor.h"

namespace
{
const char* findResource(const juce::String& filename, int& size)
{
	size = 0;
	for (int index = 0; index < RenderedUIData::namedResourceListSize; ++index)
	{
		const auto* resourceName = RenderedUIData::namedResourceList[index];
		if (filename == RenderedUIData::getNamedResourceOriginalFilename(resourceName))
			return RenderedUIData::getNamedResource(resourceName, size);
	}
	return nullptr;
}
}

RenderedUserInterface::RenderedUserInterface(SynthGuitarAudioProcessor& processor)
	: AudioProcessorEditor(processor), audioProcessor(processor)
{
	addAndMakeVisible(content);
	content.setBounds(0, 0, designWidth, designHeight);
	loadAssets();
	lastTransientCount = audioProcessor.getTransientCount();
	timerCallback();
	startTimerHz(30);
	setResizable(true, true);
	setResizeLimits(400, 540, designWidth, designHeight);
	getConstrainer()->setFixedAspectRatio(static_cast<double>(designWidth) / designHeight);
	setSize(600, 810);
}

RenderedUserInterface::~RenderedUserInterface()
{
	stopTimer();
}

void RenderedUserInterface::loadAssets()
{
	juce::String backgroundName = "background.png";
	int backgroundSize = 0;
	const auto* backgroundData = findResource(backgroundName, backgroundSize);
	if (backgroundData == nullptr)
	{
		backgroundName = "Main.png";
		backgroundData = findResource(backgroundName, backgroundSize);
	}
	if (backgroundData != nullptr)
		content.background = juce::ImageCache::getFromMemory(backgroundData, backgroundSize);
	if (content.background.isNull())
		content.errors.add("Cannot load embedded background: " + backgroundName);

	int layoutSize = 0;
	const auto* layoutData = findResource("juce_layout.json", layoutSize);
	if (layoutData == nullptr)
	{
		content.errors.add("Cannot find embedded layout: juce_layout.json");
		return;
	}

	juce::var layout;
	const auto result = juce::JSON::parse(juce::String::fromUTF8(layoutData, layoutSize), layout);
	auto* layoutObject = layout.getDynamicObject();
	if (result.failed() || layoutObject == nullptr)
	{
		content.errors.add("Invalid embedded layout: juce_layout.json " + result.getErrorMessage());
		return;
	}

	const std::pair<const char*, const char*> parameterIds[] = {
		{ "LowFrequency", SynthGuitarAudioProcessor::singleLowFrequencyParameterId },
		{ "Sensitivity", SynthGuitarAudioProcessor::singleSensitivityParameterId },
		{ "HighFrequency", SynthGuitarAudioProcessor::singleHighFrequencyParameterId },
		{ "Plectrums", SynthGuitarAudioProcessor::strumParameterId },
		{ "Strum", SynthGuitarAudioProcessor::strumParameterId },
		{ "DetectionMode", SynthGuitarAudioProcessor::detectionModeParameterId }
	};
	for (const auto& entry : layoutObject->getProperties())
	{
		const auto name = entry.name.toString();
		const auto controlName = name.startsWith("Camera_") ? name.substring(7) : juce::String();
		const bool rotaryKnob = controlName == "LowFrequency" || controlName == "Sensitivity"
			|| controlName == "HighFrequency";
		const bool indicator = controlName == "Detect" || controlName == "MidiIn"
			|| controlName == "MidiOut" || controlName == "Strings" || controlName == "TopLights";
		const char* parameterId = nullptr;
		if (name.startsWith("Camera_"))
			for (const auto& mapping : parameterIds)
				if (name.substring(7) == mapping.first)
					parameterId = mapping.second;
		if (parameterId == nullptr && !indicator)
		{
			DBG("Unmapped rendered UI control: " + name);
			continue;
		}

		auto* entryObject = entry.value.getDynamicObject();
		if (entryObject == nullptr)
		{
			content.errors.add("Invalid layout entry: " + name);
			continue;
		}
		bool validNumbers = true;
		for (const auto* key : { "x", "y", "width", "height", "frames" })
			validNumbers = validNumbers && entryObject->getProperty(key).isInt();
		if (!validNumbers)
		{
			content.errors.add("Layout requires integer x, y, width, height and frames: " + name);
			continue;
		}

		const int x = entryObject->getProperty("x");
		const int y = entryObject->getProperty("y");
		const int width = entryObject->getProperty("width");
		const int height = entryObject->getProperty("height");
		const int frames = entryObject->getProperty("frames");
		const int requiredFrames = (controlName == "Strings" || controlName == "TopLights") ? 4
			: (controlName == "DetectionMode" || controlName == "Plectrums" || controlName == "Strum") ? 3 : 2;
		if (width <= 0 || height <= 0 || frames <= 0
			|| (!rotaryKnob && frames < requiredFrames)
			|| x >= designWidth || y >= designHeight
			|| static_cast<juce::int64>(x) + width <= 0 || static_cast<juce::int64>(y) + height <= 0)
		{
			content.errors.add("Invalid bounds or frame count: " + name);
			continue;
		}

		auto imageName = name + ".png";
		int imageSize = 0;
		const auto* imageData = findResource(imageName, imageSize);
		if (imageData == nullptr)
		{
			imageName = name + "_filmstrip.png";
			imageData = findResource(imageName, imageSize);
		}
		const auto image = imageData != nullptr
			? juce::ImageCache::getFromMemory(imageData, imageSize) : juce::Image();
		if (image.isNull() || image.getHeight() < frames || image.getHeight() % frames != 0)
		{
			content.errors.add("Cannot load vertical filmstrip with " + juce::String(frames)
							   + " frames: " + imageName);
			continue;
		}

		if (rotaryKnob)
		{
			auto knob = std::make_unique<FilmstripKnob>(image, frames);
			knob->setName(name);
			knob->setBounds(x, y, width, height);
			content.addAndMakeVisible(*knob);
			auto attachment = std::make_unique<SliderAttachment>(audioProcessor.getParameters(), parameterId, *knob);
			knobs.push_back(std::move(knob));
			attachments.push_back(std::move(attachment));
		}
		else
		{
			auto control = std::make_unique<FilmstripControl>(image, frames);
			control->setName(name);
			control->setBounds(x, y, width, height);
			if (!indicator)
				control->attachChoice(*audioProcessor.getParameters().getParameter(parameterId),
					std::vector<int>{ 0, 1, 2 });
			if (controlName == "Detect") detectLight = control.get();
			if (controlName == "MidiIn") midiInLight = control.get();
			if (controlName == "MidiOut") midiOutLight = control.get();
			if (controlName == "Strings") strings = control.get();
			if (controlName == "TopLights") topLights = control.get();
			content.addAndMakeVisible(*control);
			controls.push_back(std::move(control));
		}
	}
	if (topLights != nullptr) topLights->toBack();
}

void RenderedUserInterface::timerCallback()
{
	const auto now = juce::Time::getMillisecondCounterHiRes();
	const auto count = audioProcessor.getTransientCount();
	if (count != lastTransientCount)
	{
		lastTransientCount = count;
		detectUntil = now + 100.0;
	}
	if (detectLight != nullptr) detectLight->setFrame(now < detectUntil ? 1 : 0);
	if (midiInLight != nullptr) midiInLight->setFrame(audioProcessor.isInputNoteActive() ? 1 : 0);
	const bool outputActive = audioProcessor.isOutputNoteActive();
	if (midiOutLight != nullptr) midiOutLight->setFrame(outputActive ? 1 : 0);
	if (strings != nullptr || topLights != nullptr)
	{
		const auto strum = audioProcessor.getParameters().getRawParameterValue(
			SynthGuitarAudioProcessor::strumParameterId)->load();
		using StrumMode = SynthGuitarAudioProcessor::StrumMode;
		const int frame = outputActive ? juce::jlimit(static_cast<int>(StrumMode::Low),
			static_cast<int>(StrumMode::Both), juce::roundToInt(strum)) + 1 : 0;
		if (strings != nullptr) strings->setFrame(frame);
		if (topLights != nullptr) topLights->setFrame(frame);
	}
}

void RenderedUserInterface::Content::paint(juce::Graphics& graphics)
{
	graphics.fillAll(juce::Colours::black);
	if (background.isValid())
		graphics.drawImage(background, getLocalBounds().toFloat());

	if (!errors.isEmpty())
	{
		const auto errorBounds = getLocalBounds().reduced(20).withHeight(180);
		graphics.setColour(juce::Colours::black.withAlpha(0.85f));
		graphics.fillRect(errorBounds);
		graphics.setColour(juce::Colours::white);
		graphics.setFont(juce::Font(juce::FontOptions(18.0f)));
		graphics.drawFittedText(errors.joinIntoString("\n"), errorBounds.reduced(10),
								juce::Justification::topLeft, 8);
	}
}

void RenderedUserInterface::paint(juce::Graphics& graphics)
{
	graphics.fillAll(juce::Colours::black);
}

void RenderedUserInterface::resized()
{
	const auto scale = juce::jmin(static_cast<float>(getWidth()) / designWidth,
								static_cast<float>(getHeight()) / designHeight);
	content.setTransform(juce::AffineTransform::scale(scale).translated(
		(static_cast<float>(getWidth()) - designWidth * scale) * 0.5f,
		(static_cast<float>(getHeight()) - designHeight * scale) * 0.5f));
}
