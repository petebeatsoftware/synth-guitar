# Copilot Instructions

## Project Guidelines
- For synth-guitar's pre-rendered UI, preserve SynthGuitarUserInterface and select a separate RenderedUserInterface via createEditor(). Load assets and JSON from disk at runtime, use vertical filmstrips with RotaryVerticalDrag, and keep resizing at the 800:1080 aspect ratio. Camera_LowFrequency edits singleLowFrequency.
- For synth-guitar, build and test through scripts/configure.ps1 as documented in README, rather than Visual Studio's current build configuration, to ensure the x64 MSVC environment.