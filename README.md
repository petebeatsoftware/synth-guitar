# Instrument

A JUCE-based audio/MIDI effect plugin, built with CMake. The first version is a
small “Hello World” shell: audio passes through unchanged and MIDI notes are
transposed down one semitone.

## Formats

- VST3 on Windows, macOS, and Linux
- Audio Unit (AU) on macOS
- AAX for Pro Tools is optional and requires the Avid AAX SDK and applicable
  developer access

## Build

Install the prerequisites for your platform, then run:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The `dev` preset downloads the pinned JUCE release using CMake FetchContent.
See [docs/setup.md](docs/setup.md) for platform dependencies and packaging
instructions, and [docs/structure.md](docs/structure.md) for the repository
layout.

> **Windows note:** this project only supports 64-bit (x64) MSVC builds.
> Visual Studio's CMake integration applies the x64 toolset automatically
> when you open the folder and use the `dev`/`ci` presets. If you configure
> from a plain PowerShell window instead, load the x64 developer environment
> first (for example by running `scripts/configure.ps1 -Build -Test`, or by
> launching an "x64 Native Tools Command Prompt for VS"), otherwise CMake may
> select the 32-bit host compiler and fail to link against the 64-bit SDK
> libraries.

To build an installer with CPack, run:

```sh
cmake --build --preset dev --target package
```

Available plugin formats are packaged as selectable components or component
packages, depending on the platform's installer format.
