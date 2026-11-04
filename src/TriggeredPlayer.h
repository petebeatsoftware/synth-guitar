#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <bitset>
#include <vector>

namespace instrument
{

/** Tracks held input MIDI notes and plays them when triggered.

	Playing notes stop when another trigger arrives (a new set is then played),
	when the note length elapses, or when the input note-off for that note arrives.

	Per block, call processBlock() first, then trigger() any number of times with
	increasing sample positions, then finishBlock().
*/
class TriggeredPlayer
{
public:
	static constexpr juce::uint8 kVelocity = 127;

	TriggeredPlayer() = default;

	void prepare(double sampleRate, int maximumBlockSize);

	/** Clears held and playing notes without emitting note-offs. */
	void reset();

	/** Emits note-offs for playing notes at the start of the block and clears state. */
	void releaseNotes(juce::MidiBuffer& midiOutput);

	void setNoteLengthSeconds(double seconds) noexcept { noteLengthSeconds.store(seconds); }
	double getNoteLengthSeconds() const noexcept { return noteLengthSeconds.load(); }

	/** MIDI output channel, 1 to 16. */
	void setChannel(int newChannel) noexcept { channel.store(juce::jlimit(1, 16, newChannel)); }
	int getChannel() const noexcept { return channel.load(); }

	/** Begins a block. Input note events are consumed as the block is advanced. Output must outlive finishBlock(). */
	void processBlock(const juce::MidiBuffer& input, juce::MidiBuffer& output, int numSamples);

	/** Stops playing notes and plays the currently held notes, at the given sample in the current block. */
	void trigger(int samplePosition, juce::uint8 velocity = kVelocity);

	/** Applies remaining input events and timer expiry to the end of the block. */
	void finishBlock();

	/** True while any note is playing. */
	bool isNoteActive() const noexcept { return noteActive.load(); }

private:
	struct PendingEvent
	{
		int position;
		int note;
		bool isOn;
	};

	void advanceTo(int position);
	void stopPlaying(int position);

	std::vector<PendingEvent> pending;
	size_t nextPending = 0;
	juce::MidiBuffer* output = nullptr;
	int blockSize = 0;

	std::bitset<128> held;
	std::bitset<128> playing;
	int timerEnd = -1;
	int currentChannel = 1;
	double sampleRate = 44100.0;

	std::atomic<double> noteLengthSeconds{ 0.2 };
	std::atomic<int> channel{ 1 };
	std::atomic<bool> noteActive{ false };
};

} // namespace instrument
