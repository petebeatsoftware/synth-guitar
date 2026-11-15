# Instrument

A plugin for converting a rhythm guitar track into a synth track. It doesn't generate any audio data itself,
use it with your favourite synths!

Check out my YouTube channel for a video demonstration: [https://www.youtube.com/@_pete_beat](https://www.youtube.com/@_pete_beat)

## Formats

- VST3 on Windows and Linux
- Mac support is planned, but not yet implemented. It builds but is not signed with an Apple Developer certificate

## Installation

Download the installer for your OS. On Windows, run the installer. On Linux, use your package manager to install.

![Synth Guitar interface](src/Rendered%20UI/Main.png)

# Support

If you love it, you could buy me a coffee 
[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/A1W52AQ1ZJ)

# For developers

## Configure, build, and test

On Windows just run

```
./scripts/configure.ps1 -Build -Test dev
```

## Build on other platforms

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
