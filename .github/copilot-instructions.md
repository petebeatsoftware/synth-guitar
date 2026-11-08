# Copilot Instructions

## Project Guidelines
- For synth-guitar's pre-rendered UI, preserve SynthGuitarUserInterface and select a separate RenderedUserInterface via createEditor(). Load assets and JSON from disk at runtime, use vertical filmstrips with RotaryVerticalDrag, and keep resizing at the 800:1080 aspect ratio. Camera_LowFrequency edits singleLowFrequency.
- For synth-guitar, build and test through scripts/configure.ps1 as documented in README, rather than Visual Studio's current build configuration, to ensure the x64 MSVC environment.
- For synth-guitar build and warning analysis, use the workspace terminal command `./scripts/configure.ps1 -Build -Test dev`, preserving those arguments when enabling verbosity. Never use Visual Studio build or run_build. Warning-analysis requests require analysis and a remediation plan only, with no code changes.
- For synth-guitar warning analysis, do not use MSBuild, Visual Studio builds, run_build, or VS build tools. Use only the workspace terminal with ./scripts/configure.ps1 -Build -Test dev; provide analysis and remediation plan without changing code.

## Code Style
- In tests, inline logic into check() lambdas rather than using separate helper functions called only by a single-statement check() body. Retain genuinely shared helpers.