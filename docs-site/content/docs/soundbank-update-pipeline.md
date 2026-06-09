---
title: Soundbank Update Pipeline
weight: 30
---

`scripts/update_soundbank.py` updates the fitted drone soundbank from user audio
under `audio/base`, rebuilds the JUCE plugin, and reinstalls the VST3 bundle.

`audio/base` is treated as immutable input. The script only reads source samples
from that tree. Generated outputs under `runs/`, `reports/`, `renders/`,
`assets/fitted/`, `build/`, and the user VST3 folder are considered replaceable.

## Check Status

```bash
uv run python scripts/update_soundbank.py --mode status
```

This lists discovered samples and whether each has a fitted bank in
`assets/fitted`. It also reports duplicate source files with identical SHA-256
content so accidental copies can be removed before fitting.

## Process Only New Samples

```bash
uv run python scripts/update_soundbank.py --mode new --yes
```

This processes samples whose fitted bank asset is missing, then rebuilds and
reinstalls the VST3:

- `Hemerodromos Drone.vst3`

The rebuilt plugin can host up to four synthesized drone layers per instance.
Layer `1` is enabled by default; layers `2` through `4` are available but disabled
until switched on in the plugin GUI or by the host parameter.

## Reprocess Selected Samples

```bash
uv run python scripts/update_soundbank.py \
  --mode selected \
  --samples drone_base13 drone_base14 \
  --yes
```

Selectors can be stems, filenames, or paths.

## Reprocess Everything From Scratch

```bash
uv run python scripts/update_soundbank.py --mode all --yes
```

For each selected sample, the script removes generated artifacts for that sample,
runs the fitter, checks fit quality, copies approved fitted banks to
`assets/fitted`, rebuilds the plugin, installs the VST3 bundle, and applies an
ad-hoc macOS signature.

## Serious Fitting Defaults

The default settings match the current serious fitting pass:

```text
partials:        96
steps:           300
window seconds:  1.5
basis order:     4
learning rate:   0.03
device:          auto
STFT sizes:      512,2048,8192
frequency fit:   +/-20 cents static, +/-12 cents motion
residual bands:  disabled unless --residual-noise is set
report every:    100
report seconds:  10
```

Override them directly when needed:

```bash
uv run python scripts/update_soundbank.py --mode selected --samples drone_base13 \
  --steps 600 \
  --basis-order 6 \
  --max-frequency-offset-cents 25 \
  --max-frequency-motion-cents 18 \
  --device auto \
  --yes
```

Use `--dry-run` to preview all cleanup, fit, build, install, and signing steps
without changing files.

## Profiled Fit Variants

Use `--fit-version` to let multiple fits for the same source sample coexist.
The version string becomes part of the run directory, fitted asset filename, and
embedded plugin bank profile:

```text
runs/drone_base15_fit_v001_mrstft/
assets/fitted/drone_base15_fit_v001_mrstft.dronebank.json
plugin bank: Drone 15 MR-STFT
```

The current comparison set uses:

```bash
uv run python scripts/update_soundbank.py --mode all \
  --fit-version v001_mrstft \
  --yes
```

and:

```bash
uv run python scripts/update_soundbank.py --mode all \
  --fit-version v001_srstft \
  --no-multi-resolution-stft-loss \
  --stft-fft-sizes 2048 \
  --yes
```

The `MR-STFT` profile keeps the current serious settings. The `SR-STFT` profile
changes only the STFT loss setting; sub-bin peaks, adaptive partial count,
frequency offsets, frequency motion, stereo fitting, and residual-noise-disabled
behavior remain otherwise unchanged.

## Old-Style Compatibility Refit

To keep the cleaned filenames and UI bank names but disable the newer fitting
features, use:

```bash
uv run python scripts/update_soundbank.py --mode selected --samples drone_base15 \
  --fit-version v001_compat \
  --no-sub-bin-peaks \
  --fixed-partial-count \
  --no-frequency-offsets \
  --no-frequency-motion \
  --no-multi-resolution-stft-loss \
  --no-stereo-fit \
  --no-residual-noise \
  --stft-fft-sizes 2048 \
  --yes
```

This produces the earlier style of additive bank: raw FFT-bin peak frequencies,
fixed requested partial count, amplitude-only fitting, one STFT loss size,
neutral stereo width, and no residual noise bands.

## Quality Gate And Approval

Every fit writes final comparison metrics under:

```text
runs/<sample>_fit_<fit-version>/reports/final/comparison_metrics.json
```

The updater summarizes:

- residual RMS;
- mean and median STFT dB delta;
- exported partial count.

By default the gate warns but still embeds the bank. To block poor fits:

```bash
uv run python scripts/update_soundbank.py --mode selected --samples drone_base13 \
  --quality-gate fail \
  --max-residual-rms 0.34 \
  --max-mean-stft-delta 8.0 \
  --min-partials 24 \
  --yes
```

To manually approve each fitted bank before it enters `assets/fitted`:

```bash
uv run python scripts/update_soundbank.py --mode selected --samples drone_base13 \
  --require-approval
```

To recreate the full embedded soundbank with the current model:

```bash
uv run python scripts/update_soundbank.py --mode all --duplicate-policy warn --yes
```

Use `--duplicate-policy warn` only when you intentionally want duplicate source
content fitted as separate banks.

