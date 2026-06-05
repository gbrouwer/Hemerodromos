---
title: Soundbank Update Pipeline
weight: 30
---

`scripts/update_soundbank.py` updates the fitted drone soundbank from user audio
under `audio/base`, rebuilds the JUCE plugins, and reinstalls the VST3 bundles.

`audio/base` is treated as immutable input. The script only reads source samples
from that tree. Generated outputs under `runs/`, `reports/`, `renders/`,
`assets/fitted/`, `build/`, and the user VST3 folder are considered replaceable.

## Check Status

```bash
uv run python scripts/update_soundbank.py --mode status
```

This lists discovered samples and whether each has a fitted bank in
`assets/fitted`.

## Process Only New Samples

```bash
uv run python scripts/update_soundbank.py --mode new --yes
```

This processes samples whose fitted bank asset is missing, then rebuilds and
reinstalls both VST3s:

- `Hemerodromos Drone.vst3`
- `Hemerodromos Drone Triggered.vst3`

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
runs the fitter, copies the fitted bank to `assets/fitted`, rebuilds the plugins,
installs the VST3 bundles, and applies an ad-hoc macOS signature.

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
report every:    100
report seconds:  10
```

Override them directly when needed:

```bash
uv run python scripts/update_soundbank.py --mode selected --samples drone_base13 \
  --steps 600 \
  --basis-order 6 \
  --device auto \
  --yes
```

Use `--dry-run` to preview all cleanup, fit, build, install, and signing steps
without changing files.

