#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <bitset>

namespace instrument
{

/** Splits one MIDI stream into two.

	The lowest held note goes to output 0. All other notes go to output 1.
	All non-note messages go to every output. When a new note is lower than every
	held note, the previous lowest note is moved from output 0 to output 1.
	Releasing the lowest note does not promote another held note.
*/
class MidiSplitter
{
public:
	static constexpr int kNumOutputs = 2;

	MidiSplitter() = default;

	/** Clears all outputs, then fills them from the input. */
	void processData(const juce::MidiBuffer& input, std::array<juce::MidiBuffer, kNumOutputs>& outputs);

	/** Forgets all held notes. */
	void reset();

	/** Adds note-ons for the notes currently routed to the given output. */
	void addHeldNotes(int outputIndex, juce::MidiBuffer& destination, int position) const;

private:
	static void addToUpper(std::array<juce::MidiBuffer, kNumOutputs>& outputs,
						   const juce::MidiMessage& message, int position);

	std::bitset<128> held;
	std::array<juce::uint8, 128> velocities{};
	std::array<juce::uint8, 128> channels{};
	int lowNote = -1;
};

} // namespace instrument
