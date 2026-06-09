---
title: Beta Release
weight: 5
---

This repository now contains the first beta of the Hemerodromos Drone VST3
instrument.

## Plugin

The beta plugin is `Hemerodromos Drone`, a JUCE Standalone and VST3 instrument
that renders fitted drone-bank JSON assets in real time. The plugin is built as
a MIDI-triggered synth and installs on macOS to:

```text
~/Library/Audio/Plug-Ins/VST3/Hemerodromos Drone.vst3
```

The current interface uses the `UMOJA` / `DR-001` branding, a rack-style light
and dark panel, Audiowide UI typography, equal-size rotary controls with LED
value rings, and per-knob neutral reset switches.

## Sound Engine

The beta sound engine supports up to four independent drone layers per plugin
instance. Each layer has:

- its own enabled state;
- its own fitted-bank selection;
- its own profile selection, currently used for `MR-STFT` and `SR-STFT` fits;
- its own neutral-centered synthesis and post-processing parameters.

Layer 1 is enabled by default. Layers 2 through 4 are available but disabled
until enabled in the GUI or by the host.

## Presets

The plugin can save and load `.hmdpreset` files. Presets store the selected
drone banks, enabled layer states, and per-layer parameter values.

## Fitted Soundbank

The embedded beta soundbank uses profile-aware fitted assets under
`assets/fitted/`, with filenames such as:

```text
drone_base15_fit_v001_mrstft.dronebank.json
drone_base15_fit_v001_srstft.dronebank.json
```

The updater treats `audio/base/` as immutable source material and treats
generated artifacts under `runs/`, `reports/`, `renders/`, `assets/fitted/`,
`build/`, and the installed VST3 bundle as replaceable derivatives.

## Fitting Pipeline

The default serious fitting profile keeps residual/noise-band synthesis disabled
for the embedded VST3 banks. The current fitting/export path includes:

- duplicate source detection by SHA-256;
- cleaned bank names for the UI;
- profile-aware output naming;
- sub-bin peak interpolation;
- adaptive partial count;
- small fitted frequency offsets;
- slow frequency motion;
- fitted stereo-width metadata;
- fit-quality metrics and optional approval before embedding.

The compatibility switches documented in `dronefit-fitting.md` and
`soundbank-update-pipeline.md` can still disable those newer fitting features
when an old-style additive refit is needed.

## Font License

Audiowide is bundled from Google Fonts and is licensed under the SIL Open Font
License. The license copy is stored at:

```text
plugin/Assets/fonts/OFL-Audiowide.txt
```

## Beta Expectations

This is a working beta rather than a finalized commercial plugin. The expected
workflow is to use Ableton or the Standalone build for musical testing, then
iterate on fit quality, GUI polish, and parameter behavior from actual listening
notes.

