---
title: Full Walkthrough
weight: 10
---

This walkthrough covers the full local workflow for Hemerodromos:

- install the Python and C++ toolchain;
- verify macOS and PyTorch/MPS capability;
- add source samples to the protected soundbank input folder;
- fit new drone banks;
- rebuild the Standalone app and VST3 plugin;
- install the rebuilt VST3 bundle for Ableton or another DAW.

The protected source-audio area is:

```text
audio/base/
```

Treat that folder as the inner sanctum. Put user-provided source samples there,
but do not let scripts modify or delete anything inside it. Generated outputs in
`runs/`, `reports/`, `renders/`, `assets/fitted/`, `build/`, and the VST3 install
folder are derivative and replaceable.

## 1. Clone And Enter The Repo

```bash
git clone <repo-url> Hemerodromos
cd Hemerodromos
git submodule update --init --recursive
```

The JUCE dependency and the Hextra docs theme are Git submodules. The recursive
submodule command is required before building the plugin or the documentation
site from a fresh clone.

## 2. Install Local Tools

Install `uv` and the macOS compiler tools:

```bash
brew install uv cmake ninja
xcode-select --install
```

Then create or refresh the Python environment:

```bash
uv sync --dev
```

The project currently uses Python 3.12 and PyTorch with Apple MPS acceleration
when available.

## 3. Verify Capabilities

Run the capability check:

```bash
uv run python scripts/check_capabilities.py --strict
```

Expected high-level result on Apple Silicon:

```text
All checks passed!
PyTorch acceleration: MPS available
Recommended device: mps
```

If `pluginval` is missing, that is acceptable for the current workflow. The VST3
build and code-sign checks still run.

## 4. Understand The Existing Pipeline

The offline fitting tool is `dronefit`. Its serious fitting defaults are:

```text
partials:        96
steps:           300
window seconds:  1.5
basis order:     4
learning rate:   0.03
device:          auto
STFT sizes:      512,2048,8192
frequency fit:   +/-20 cents static, +/-12 cents motion
residual bands:  disabled
report every:    100
report seconds:  10
```

For one sample, the manual equivalent is:

```bash
uv run dronefit fit audio/base/drones/drone_base13.wav \
  --out runs/drone_base13_fit_v001 \
  --partials 96 \
  --steps 300 \
  --window-seconds 1.5 \
  --basis-order 4 \
  --learning-rate 0.03 \
  --max-frequency-offset-cents 20 \
  --max-frequency-motion-cents 12 \
  --device auto \
  --report-every 100 \
  --report-seconds 10 \
  --stft-fft-sizes 512,2048,8192 \
  --overwrite
```

The soundbank update script wraps that manual flow and then rebuilds the plugin.
For a compatibility refit that keeps the cleaned filenames but disables newer
analysis features, add `--no-sub-bin-peaks --fixed-partial-count
--no-frequency-offsets --no-frequency-motion --single-resolution-stft-loss
--no-stereo-fit --no-residual-noise --stft-fft-sizes 2048` to the manual
`dronefit fit` command.

To keep multiple fits for one source sample selectable in the plugin, encode the
profile in `--fit-version`. For example, `v001_mrstft` creates
`runs/drone_base13_fit_v001_mrstft/`,
`assets/fitted/drone_base13_fit_v001_mrstft.dronebank.json`, and a plugin bank
named `Drone 13 MR-STFT`.

## 5. Add New Source Samples

Add new drone source files under `audio/base`, usually:

```text
audio/base/drones/drone_base13.wav
audio/base/drones/drone_base14.wav
```

Use clear, unique stems. The updater maps:

```text
audio/base/drones/drone_base13.wav
```

to:

```text
assets/fitted/drone_base13_fit_v001.dronebank.json
runs/drone_base13_fit_v001/
```

The source WAVs are ignored by Git by default. This is intentional.

## 6. Check What Needs Processing

```bash
scripts/update_soundbank.py --mode status
```

This scans `audio/base`, detects samples with missing fitted-bank assets, prints
a status table, and reports duplicate source files with identical content.

For a non-mutating preview:

```bash
scripts/update_soundbank.py --mode new --dry-run
```

## 7. Process Only New Samples

```bash
scripts/update_soundbank.py --mode new --yes
```

For each new source sample, the script:

1. removes derivative outputs for that sample only;
2. runs `dronefit fit`;
3. prints fit-quality metrics;
4. copies approved `runs/<sample>_fit_v001/default.dronebank.json` into `assets/fitted/`;
5. rebuilds the JUCE plugin target;
6. installs the VST3 bundle;
7. ad-hoc signs and verifies the installed VST3 bundle on macOS.

It refuses to remove paths inside `audio/base`.

## 8. Reprocess Selected Samples

```bash
scripts/update_soundbank.py \
  --mode selected \
  --samples drone_base13 drone_base14 \
  --yes
```

Use this when you want to replace only specific fitted banks.
Add `--no-sub-bin-peaks --fixed-partial-count --no-frequency-offsets
--no-frequency-motion --no-multi-resolution-stft-loss --no-stereo-fit
--no-residual-noise --stft-fft-sizes 2048` when you want the old-style
amplitude-only additive output while preserving the cleaned asset names.

For the current A/B comparison profiles:

```bash
scripts/update_soundbank.py --mode all --fit-version v001_mrstft --yes
scripts/update_soundbank.py --mode all --fit-version v001_srstft \
  --no-multi-resolution-stft-loss \
  --stft-fft-sizes 2048 \
  --yes
```

These profiles differ only by the multi-resolution STFT loss setting.

## 9. Reprocess Everything

```bash
scripts/update_soundbank.py --mode all --duplicate-policy warn --yes
```

This replaces every generated fit for every sample under `audio/base`. Use it
only when the fitting method or soundbank format has changed.

Use `--duplicate-policy warn` only if duplicate source content is intentional.
Without it, duplicate files are blocked before fitting.

## 10. Build Plugins Without Refitting

The updater rebuilds automatically after fitting. To rebuild manually:

```bash
cmake --build build
```

Build outputs are:

```text
build/HemerodromosDrone_artefacts/RelWithDebInfo/Standalone/Hemerodromos Drone.app
build/HemerodromosDrone_artefacts/RelWithDebInfo/VST3/Hemerodromos Drone.vst3
```

The plugin exposes four independent drone layers. Use the layer buttons `1`
through `4` to choose which layer the shared knobs edit. Each layer has its own
bank selector, enable switch, and neutral-centered knob values; only enabled
layers contribute sound. By default layer 1 is enabled and layers 2 through 4 are
disabled, so a new instance still behaves like a single-drone instrument until
extra layers are enabled.

## 11. Install VST3 Bundles Manually

The updater handles this automatically, but the manual install is:

```bash
ditto "build/HemerodromosDrone_artefacts/RelWithDebInfo/VST3/Hemerodromos Drone.vst3" \
  "$HOME/Library/Audio/Plug-Ins/VST3/Hemerodromos Drone.vst3"
```

Then sign and verify:

```bash
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/VST3/Hemerodromos Drone.vst3"
codesign --verify --deep --strict "$HOME/Library/Audio/Plug-Ins/VST3/Hemerodromos Drone.vst3"
```

Restart or rescan Ableton after installing the new VST3 bundle.

## 12. Build The Web Docs

The documentation site is a Hugo site in `docs-site/` using the Hextra theme.
The source docs are in `docs/`.

Refresh generated Hugo content:

```bash
scripts/rebuild_docs_site.py
```

Build the static site:

```bash
scripts/build_docs_site.sh
```

Serve locally:

```bash
scripts/serve_docs_site.sh
```

If local Hugo is missing, install it:

```bash
brew install hugo
```

GitLab Pages builds the same `docs-site/` content and publishes `public/`.

## Static Walkthrough Artifacts

The following copied artifacts are static examples for documentation only. They
are not used by the fitting or plugin build pipeline.

![Example waveform](/walkthrough-assets/drone_base2/waveform.png)

![Example spectrogram](/walkthrough-assets/drone_base2/spectrogram_log.png)

![Example fitted-vs-target spectrum](/walkthrough-assets/drone_base2/median_spectrum_overlay.png)

![Example spectrogram delta](/walkthrough-assets/drone_base2/spectrogram_delta_db.png)

The example fitted bank is copied to:

```text
docs//walkthrough-assets/drone_base2/drone_base2_fit_v001.dronebank.json
```

