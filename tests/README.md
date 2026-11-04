# Unit tests

The tests use JUCE UnitTest with a named section for each behavior. CTest runs one category per production class (plus the MIDI transposition utility). No external mocking library is required.

## Migration and coverage map

| Original checks | Suite | Focused behaviors |
| --- | --- | --- |
| Mixed MIDI transposition/preservation | MidiTransposer | Note-on, note-off, velocity-zero note-on, channel/velocity/timing, controller, SysEx, upper boundary |
| Note range | MidiTransposer | Lower boundary; empty input and scratch reuse added |
| Audio output is silent | SynthGuitarAudioProcessor | Every output sample is zero; mono/stereo channels preserved in single/multi modes across multiple rates and block sizes |
| Splitter routing | MidiSplitter | First note, higher note, lower-note displacement, note-off routing |
| No promotion | MidiSplitter | Releasing lowest does not promote upper notes |
| Velocity zero and other messages | MidiSplitter | Velocity-zero release, broadcast data, next lowest after release |
| Splitter reset | MidiSplitter | Held-note reset; held-note replay and stale output clearing added |
| Detector silence | TransientDetector | Silence in spectral, envelope and hybrid modes |
| Timing and deduplication | TransientDetector | Hybrid deduplication, envelope timing, spectral latency |
| Remaining detector scenarios | TransientDetector | Refractory period, sustained tone, reset equivalence, spectral fallback, timed note-off, active state, retrigger ordering, sensitivity, tonal onset |

## Additional class coverage

- TriggeredPlayer: defaults, channel clamping, no-trigger behavior, held chords, event ordering, input note-off, velocity-zero release, retrigger ordering, within/across-block expiry, minimum duration, release/reset and prepare fallback.
- TransientDetector: configuration clamping/defaults, empty buffers, stereo cancellation, alternate sample rates, frequency cutoff limits.
- SynthGuitarAudioProcessor: metadata/program API, bus layouts, parameter defaults/ranges/conversion, all-parameter state round trip, invalid state rejection, silence/empty blocks, lifecycle reset, single/multi mode routing and release.
- SynthGuitarUserInterface: factory/ownership, initial size, minimum/maximum layout, offscreen painting, single/multi controls, live mode switching, parameter attachments in both directions. Tests use public child components rather than private-access hacks. The live mode-switch check services JUCE timers on the message thread with a two-second timeout per transition.

## Running

Configure/build with the existing dev or ci CMake presets, then run `ctest --preset dev --output-on-failure` (or `ci`).

Run a single suite with `ctest --test-dir build -R instrument_triggered_player --output-on-failure`. The executable also accepts a category name, e.g. `instrument_tests TriggeredPlayer`. Unknown categories fail instead of silently running no tests. All suites run with a fixed seed; GUI tests initialize JUCE and paint offscreen without opening windows.
