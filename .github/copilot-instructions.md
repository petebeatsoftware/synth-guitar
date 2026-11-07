# Copilot Instructions

## Project Guidelines
- For synth-guitar's pre-rendered UI, preserve SynthGuitarUserInterface and select a separate RenderedUserInterface via createEditor(). Load assets and JSON from memory using JUCE juce_add_binary_data (RenderedUIData) without disk overrides. Increased build time to embed the images is acceptable. Use vertical filmstrips with RotaryVerticalDrag, and keep resizing at the 800:1080 aspect ratio. Camera_LowFrequency edits singleLowFrequency.