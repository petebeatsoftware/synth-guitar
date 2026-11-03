#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace instrument
{
void transposeMidiDownOneSemitone(juce::MidiBuffer& midiMessages,
                                  juce::MidiBuffer& scratchBuffer);
}
