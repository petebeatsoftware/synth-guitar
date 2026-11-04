#include "MidiSplitter.h"

#include "TriggeredPlayer.h"

namespace instrument
{

void MidiSplitter::addToUpper(std::array<juce::MidiBuffer, kNumOutputs>& outputs,
							  const juce::MidiMessage& message, int position)
{
	outputs[1].addEvent(message, position);
	outputs[2].addEvent(message, position);
}

void MidiSplitter::reset()
{
	held.reset();
	velocities.fill(0);
	channels.fill(0);
	lowNote = -1;
}

void MidiSplitter::addHeldNotes(int outputIndex, juce::MidiBuffer& destination, int position) const
{
	for (int note = 0; note < 128; ++note)
	{
		const auto index = static_cast<size_t>(note);
		if (!held.test(index) || ((note == lowNote) != (outputIndex == 0)))
			continue;
		destination.addEvent(juce::MidiMessage::noteOn(juce::jmax(1, static_cast<int>(channels[index])), note,
													   velocities[index]),
							 position);
	}
}

void MidiSplitter::processData(const juce::MidiBuffer& input,
							   std::array<juce::MidiBuffer, kNumOutputs>& outputs)
{
	for (auto& output : outputs)
		output.clear();

	for (const auto metadata : input)
	{
		const auto message = metadata.getMessage();
		const auto position = metadata.samplePosition;

		if (message.isNoteOn(false))
		{
			const auto note = message.getNoteNumber();
			const auto index = static_cast<size_t>(note);

			bool isLowest = true;
			for (int other = 0; other < note; ++other)
			{
				if (held.test(static_cast<size_t>(other)))
				{
					isLowest = false;
					break;
				}
			}

			if (isLowest)
			{
				if (lowNote >= 0 && lowNote != note)
				{
					const auto oldIndex = static_cast<size_t>(lowNote);
					const int oldChannel = channels[oldIndex];
					outputs[0].addEvent(juce::MidiMessage::noteOff(oldChannel, lowNote), position);
					addToUpper(outputs,
							   juce::MidiMessage::noteOn(oldChannel, lowNote, velocities[oldIndex]),
							   position);
				}
				lowNote = note;
				outputs[0].addEvent(message, position);
			}
			else
			{
				addToUpper(outputs, message, position);
			}

			held.set(index);
			velocities[index] = message.getVelocity() > 0 ? static_cast<juce::uint8>(message.getVelocity())
														  : TriggeredPlayer::kVelocity;
			channels[index] = static_cast<juce::uint8>(message.getChannel());
		}
		else if (message.isNoteOff(true))
		{
			const auto note = message.getNoteNumber();

			if (note == lowNote)
			{
				outputs[0].addEvent(message, position);
				lowNote = -1;
			}
			else
			{
				addToUpper(outputs, message, position);
			}
			held.reset(static_cast<size_t>(note));
		}
		else
		{
			for (auto& output : outputs)
				output.addEvent(message, position);
		}
	}
}

} // namespace instrument
