#include "TriggeredPlayer.h"

#include <limits>

namespace instrument
{

namespace
{
constexpr size_t kMaxPendingEvents = 4096;
}

void TriggeredPlayer::prepare(double newSampleRate, int maximumBlockSize)
{
	juce::ignoreUnused(maximumBlockSize);
	sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
	pending.reserve(kMaxPendingEvents);
	reset();
}

void TriggeredPlayer::reset()
{
	pending.clear();
	nextPending = 0;
	output = nullptr;
	blockSize = 0;
	held.reset();
	playing.reset();
	timerEnd = -1;
	noteActive.store(false);
}

void TriggeredPlayer::releaseNotes(juce::MidiBuffer& midiOutput)
{
	for (int note = 0; note < 128; ++note)
		if (playing.test(static_cast<size_t>(note)))
			midiOutput.addEvent(juce::MidiMessage::noteOff(currentChannel, note), 0);
	reset();
}

void TriggeredPlayer::processBlock(const juce::MidiBuffer& input, juce::MidiBuffer& newOutput, int numSamples)
{
	pending.clear();
	nextPending = 0;
	output = &newOutput;
	blockSize = numSamples;
	currentChannel = channel.load();

	for (const auto metadata : input)
	{
		const auto message = metadata.getMessage();
		if ((!message.isNoteOn() && !message.isNoteOff()) || pending.size() >= kMaxPendingEvents)
			continue;
		pending.push_back({ metadata.samplePosition, message.getNoteNumber(), message.isNoteOn() });
	}
}

void TriggeredPlayer::stopPlaying(int position)
{
	if (output == nullptr)
		return;

	for (int note = 0; note < 128; ++note)
		if (playing.test(static_cast<size_t>(note)))
			output->addEvent(juce::MidiMessage::noteOff(currentChannel, note), position);
	playing.reset();
	noteActive.store(false);
}

void TriggeredPlayer::advanceTo(int position)
{
	for (;;)
	{
		const int nextEvent = nextPending < pending.size() ? pending[nextPending].position
														   : std::numeric_limits<int>::max();

		if (timerEnd >= 0 && timerEnd <= position && timerEnd <= nextEvent)
		{
			stopPlaying(timerEnd);
			timerEnd = -1;
			continue;
		}

		if (nextEvent > position)
			break;

		const auto event = pending[nextPending++];
		const auto index = static_cast<size_t>(event.note);
		if (event.isOn)
		{
			held.set(index);
		}
		else
		{
			held.reset(index);
			if (playing.test(index) && output != nullptr)
			{
				output->addEvent(juce::MidiMessage::noteOff(currentChannel, event.note), event.position);
				playing.reset(index);
				noteActive.store(playing.any());
			}
		}
	}
}

void TriggeredPlayer::trigger(int samplePosition, juce::uint8 velocity)
{
	if (output == nullptr)
		return;

	const int position = juce::jlimit(0, juce::jmax(0, blockSize - 1), samplePosition);
	advanceTo(position);
	stopPlaying(position);

	for (int note = 0; note < 128; ++note)
		if (held.test(static_cast<size_t>(note)))
			output->addEvent(juce::MidiMessage::noteOn(currentChannel, note,
				static_cast<juce::uint8>(juce::jlimit(1, 127, static_cast<int>(velocity)))), position);

	playing = held;
	noteActive.store(playing.any());
	timerEnd = playing.any() ? position + juce::jmax(1, static_cast<int>(noteLengthSeconds.load() * sampleRate)) : -1;
}

void TriggeredPlayer::finishBlock()
{
	if (output == nullptr)
		return;

	advanceTo(juce::jmax(0, blockSize - 1));
	if (timerEnd >= 0)
		timerEnd -= blockSize;
	pending.clear();
	nextPending = 0;
	output = nullptr;
}

} // namespace instrument
