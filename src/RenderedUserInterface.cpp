#include "RenderedUserInterface.h"

#include "SynthGuitarAudioProcessor.h"

RenderedUserInterface::RenderedUserInterface(SynthGuitarAudioProcessor& processor)
	: AudioProcessorEditor(processor), audioProcessor(processor)
{
	addAndMakeVisible(content);
	content.setBounds(0, 0, designWidth, designHeight);
	loadAssets(juce::File(SYNTHGUITAR_RENDERED_UI_DIR));
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

void RenderedUserInterface::loadAssets(const juce::File& directory)
{
	auto backgroundFile = directory.getChildFile("background.png");
	if (!backgroundFile.existsAsFile())
		backgroundFile = directory.getChildFile("Main.png");
	content.background = juce::ImageFileFormat::loadFrom(backgroundFile);
	if (content.background.isNull())
		content.errors.add("Cannot load background: " + backgroundFile.getFullPathName());

	const auto layoutFile = directory.getChildFile("juce_layout.json");
	if (!layoutFile.existsAsFile())
	{
		content.errors.add("Cannot find layout: " + layoutFile.getFullPathName());
		return;
	}

	juce::var layout;
	const auto result = juce::JSON::parse(layoutFile.loadFileAsString(), layout);
	auto* layoutObject = layout.getDynamicObject();
	if (result.failed() || layoutObject == nullptr)
	{
		content.errors.add("Invalid layout: " + layoutFile.getFullPathName() + " " + result.getErrorMessage());
		return;
	}

	const std::pair<const char*, const char*> parameterIds[] = {
		{ "LowFrequency", SynthGuitarAudioProcessor::singleLowFrequencyParameterId },
		{ "Sensitivity", SynthGuitarAudioProcessor::singleSensitivityParameterId },
		{ "HighFrequency", SynthGuitarAudioProcessor::singleHighFrequencyParameterId },
		{ "Plectrums", SynthGuitarAudioProcessor::strumParameterId },
		{ "DetectionMode", SynthGuitarAudioProcessor::detectionModeParameterId }
	};
	for (const auto& entry : layoutObject->getProperties())
	{
		const auto name = entry.name.toString();
		const auto controlName = name.startsWith("Camera_") ? name.substring(7) : juce::String();
		const bool rotaryKnob = controlName == "LowFrequency" || controlName == "Sensitivity"
			|| controlName == "HighFrequency";
		const bool indicator = controlName == "Detect" || controlName == "MidiIn"
			|| controlName == "MidiOut" || controlName == "Strings";
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
		const int requiredFrames = controlName == "Strings" ? 4 : controlName == "DetectionMode" ? 3 : 2;
		if (width <= 0 || height <= 0 || frames <= 0
			|| (!rotaryKnob && frames < requiredFrames)
			|| x >= designWidth || y >= designHeight
			|| static_cast<juce::int64>(x) + width <= 0 || static_cast<juce::int64>(y) + height <= 0)
		{
			content.errors.add("Invalid bounds or frame count: " + name);
			continue;
		}

		auto imageFile = directory.getChildFile(name + ".png");
		if (!imageFile.existsAsFile())
			imageFile = directory.getChildFile(name + "_filmstrip.png");
		const auto image = juce::ImageFileFormat::loadFrom(imageFile);
		if (image.isNull() || image.getHeight() < frames || image.getHeight() % frames != 0)
		{
			content.errors.add("Cannot load vertical filmstrip with " + juce::String(frames)
							   + " frames: " + imageFile.getFullPathName());
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
					controlName == "Plectrums" ? std::vector<int>{ 0, 1 } : std::vector<int>{ 0, 1, 2 });
			if (controlName == "Detect") detectLight = control.get();
			if (controlName == "MidiIn") midiInLight = control.get();
			if (controlName == "MidiOut") midiOutLight = control.get();
			if (controlName == "Strings") strings = control.get();
			content.addAndMakeVisible(*control);
			controls.push_back(std::move(control));
		}
	}
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
	if (strings != nullptr) strings->setFrame(outputActive ? 3 : 0);
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
