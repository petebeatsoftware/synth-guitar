# Copilot Instructions

Building and testing

```
.\scripts\configure.ps1 -Build -Test dev
```

## Project Guidelines
- For synth-guitar's pre-rendered UI, preserve SynthGuitarUserInterface and select a separate RenderedUserInterface via createEditor(). Load assets and JSON from disk at runtime, use vertical filmstrips with RotaryVerticalDrag, and keep resizing at the 800:1080 aspect ratio. Camera_LowFrequency edits singleLowFrequency.
- In the Blender filmstrip generator, set Camera Resolution X to control horizontal crop width directly: reducing it narrows the view while leaving vertical framing unchanged.