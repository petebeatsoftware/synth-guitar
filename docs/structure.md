# Repository structure

This document describes the source tree and how its parts fit together.

```text
.
├── .github/workflows/       Continuous integration and release automation
├── cmake/                   CPack installer and plugin component rules
├── docs/                    Developer setup and repository guide
├── src/                     JUCE processor, editor, and MIDI processing
├── tests/                   Processor behavior tests
├── CMakeLists.txt           Project, JUCE, plugin, and test targets
├── CMakePresets.json        Reproducible development and CI build presets
└── README.md                Project summary and quick-start commands
```

## Build and dependency flow

`CMakeLists.txt` fetches the pinned JUCE 8.0.6 source with FetchContent, defines
the platform-appropriate plugin formats, then builds the shared `instrument`
processor and its wrappers. It also defines a console test target using the
same processor and processing code. `CMakePresets.json` standardizes local and
CI configurations; generated files stay in ignored build directories.

## Source

- `src/PluginProcessor.h/.cpp`: JUCE `AudioProcessor`; declares matching mono
  or stereo audio buses, passes audio through, and routes MIDI through the
  transposer.
- `src/PluginEditor.h/.cpp`: minimal resizable JUCE editor.
- `src/MidiTransposer.h/.cpp`: reusable MIDI event transformation. It shifts
  note-on/note-off events down one semitone, clamps note zero at zero, and
  preserves event timing and other MIDI messages.

## Tests

`tests/MidiTransposerTests.cpp` exercises MIDI note-on/off and velocity-zero
note-on handling, controller/SysEx preservation, timestamps, the low-note
boundary, and exact audio passthrough. CTest runs the test executable.

## Packaging

`cmake/Packaging.cmake` installs each built plugin format as its own CPack
component and selects a platform package generator. Windows uses an NSIS
component installer when NSIS is installed (otherwise component ZIP files);
macOS produces component packages; Linux produces component DEB packages.
AAX is excluded unless explicitly enabled with a separately installed Avid
SDK. Signing and notarization are deployment responsibilities, not configured
in the initial shell.

## Automation

`.github/workflows/ci.yml` builds, tests, packages, and uploads platform
artifacts on Windows, macOS, and Linux. Tag builds also attach those artifacts
to a GitHub Release. AAX is not built on hosted runners because its SDK is
licensed and must be provisioned separately.

## Setup guide

`docs/setup.md` lists compiler/toolchain requirements, platform dependencies,
build and test commands, package behavior, optional AAX setup, and common
troubleshooting steps.
