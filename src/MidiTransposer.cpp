#include "MidiTransposer.h"

namespace instrument
{
void transposeMidiDownOneSemitone(juce::MidiBuffer& midiMessages,
                                  juce::MidiBuffer& scratchBuffer)
{
    scratchBuffer.clear();

    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOn(true) || message.isNoteOff(false))
            message.setNoteNumber(juce::jmax(0, message.getNoteNumber() - 1));

        const auto added = scratchBuffer.addEvent(message, metadata.samplePosition);
        jassert(added);
        juce::ignoreUnused(added);
    }

    midiMessages.swapWith(scratchBuffer);
    scratchBuffer.clear();
}
}
