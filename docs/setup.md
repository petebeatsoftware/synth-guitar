# Developer setup

This project uses C++17, CMake, Ninja, and JUCE 8.0.6. CMake FetchContent
downloads JUCE at configure time; the project does not vendor JUCE.
Before distributing the plugin, review JUCE's current licensing terms and
select the appropriate JUCE license for the product.

## Required tools

Install the following for your operating system:

- Git
- CMake 3.22 or newer
- Ninja
- A C++17 compiler and platform SDK
- JUCE's platform dependencies listed below

### Windows

- Visual Studio 2022 with **Desktop development with C++**
- A Windows 10/11 SDK
- CMake and Ninja (the Visual Studio installer can provide CMake)
- NSIS to generate the selectable-format installer; without it, CPack creates
  component ZIP archives instead

Run builds from a Visual Studio Developer PowerShell or a shell where the
Visual Studio compiler is available to Ninja.

### macOS

- Xcode and its command-line tools (`xcode-select --install`)
- CMake and Ninja, for example through Homebrew
- An Apple developer account and signing/notarization credentials for
  distributing signed installers

The project builds VST3 and AU on macOS. AAX requires separate Avid developer
access and SDK setup.

### Linux

Install a C++ compiler, CMake, Ninja, and the JUCE GUI/audio development
dependencies. On Ubuntu/Debian, a starting set is:

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config \
  libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev \
  libgtk-3-dev libx11-dev \
  libxcomposite-dev libxcursor-dev libxinerama-dev libxrandr-dev \
  libxrender-dev libglu1-mesa-dev
```

Distribution package names can vary. VST3 is the initial Linux plugin format.

## Configure, build, and test

From the repository root:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The first configure fetches JUCE from GitHub, so it requires network access.
The `build/` directory contains generated files and is ignored by Git.

For CI-like optimized builds:

```sh
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
```

Use your DAW's plugin rescanning/validation tools to load the resulting VST3
or AU. JUCE plugin outputs are under the selected build directory; target
locations vary by platform and generator.

## Packaging

Run:

```sh
cmake --build --preset dev --target package
```

CPack uses NSIS on Windows when available, `productbuild` on macOS, and DEB on
Linux. NSIS presents format components for selection. On macOS, CPack emits
component packages that users can select during installation; Linux DEB
components are separate packages selected with the system package manager.
If NSIS is unavailable, the Windows output is a ZIP per component.

The installer only includes formats configured on the build host. Code
signing/notarization is not configured in the starter project and must be
added for public distribution.

### Optional AAX build

Install the Avid AAX SDK through Avid's developer program and observe its
license terms. Then configure with the SDK path:

```sh
cmake -S . -B build-aax -G Ninja \
  -DINSTRUMENT_ENABLE_AAX=ON \
  -DJUCE_AAX_SDK_PATH="C:/path/to/AAX_SDK"
cmake --build build-aax
```

AAX is supported only on Windows and macOS here. Do not commit the SDK or
credentials. An AAX-enabled build must run in a properly provisioned,
authorized environment.

## Troubleshooting

- **CMake cannot find Ninja/compiler:** verify both are on `PATH`; on Windows
  start a Visual Studio Developer shell.
- **JUCE fetch fails:** check GitHub connectivity and retry configuration.
- **Missing Linux headers/libraries:** install the platform development
  packages listed above, then clear the build directory and configure again.
- **AAX target is unavailable:** confirm Avid developer access, SDK setup,
  supported platform, and `JUCE_AAX_SDK_PATH`.
- **DAW does not show the plugin:** confirm the plugin format and install
  directory are supported by the host, then trigger a rescan.
