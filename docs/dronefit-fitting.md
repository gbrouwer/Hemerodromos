# Dronefit Fitting

The first fitting implementation is an interpretable PyTorch optimizer, not a
runtime neural network for the plugin.

It starts from an initialized `.dronebank.json` and optimizes:

- partial base amplitudes;
- a folded global gain term;
- slow Fourier amplitude-motion coefficients per partial;
- small static frequency offsets;
- slow Fourier frequency-motion coefficients per partial;
- compact stereo-width metadata;
- optional residual noise-band metadata when explicitly enabled.

The exported bank remains portable JSON and can be rendered by `dronefit render`
without PyTorch.

## Fit Command

Use an existing initialized bank:

```bash
uv run dronefit fit audio/base/drones/drone_base3.wav \
  --init-bank runs/drone_base3_init/default.dronebank.json \
  --out runs/drone_base3_fit_v001 \
  --steps 300 \
  --window-seconds 1.5 \
  --basis-order 4 \
  --learning-rate 0.03 \
  --max-frequency-offset-cents 20 \
  --max-frequency-motion-cents 12 \
  --device auto \
  --report-every 100 \
  --report-seconds 10 \
  --stft-fft-sizes 512,2048,8192
```

Or let `fit` create the initial bank:

```bash
uv run dronefit fit audio/base/drones/drone_base3.wav \
  --out runs/drone_base3_fit_v001 \
  --partials 96
```

## Outputs

The fit output directory contains:

- `initial.dronebank.json` when no initial bank was supplied;
- `default.dronebank.json` for the fitted bank;
- `<run-directory-name>.wav` for the fitted render;
- `checkpoints/step_*.dronebank.json`;
- `reports/step_*/` checkpoint figures;
- `reports/final/` final comparison figures and metrics.

Example:

```text
runs/drone_base4_fit_v001/default.dronebank.json
runs/drone_base4_fit_v001/drone_base4_fit_v001.wav
runs/drone_base4_fit_v001/reports/final/
```

`dronefit fit` refuses to write into an existing non-empty output directory by
default. Use a new run directory for each serious experiment:

```text
runs/drone_base4_fit_v001
runs/drone_base4_fit_v002
runs/drone_base4_fit_v003
```

If a run should intentionally be replaced, pass:

```bash
--overwrite
```

With `--overwrite`, `dronefit` clears known generated fit artifacts from that
run directory before writing new outputs.

The fitted bank uses:

```text
model_type: additive_fourier_amp_freq_controls
basis.type: fourier
basis.order: configured --basis-order
stereo_width: fitted source-width estimate
```

Each partial receives optimized `amp_coefficients`,
`freq_log_ratio_coefficients`, and a small adjusted `freq_hz`. These
coefficients represent learned motion paths that VST3 macro controls can scale
or traverse at different rates.

## Compatibility Switches

The newer fitting features can be disabled without changing filename hygiene,
run directory names, asset names, or cleaned bank display names:

```bash
uv run dronefit fit audio/base/drones/drone_base3.wav \
  --out runs/drone_base3_fit_v001 \
  --partials 96 \
  --no-sub-bin-peaks \
  --fixed-partial-count \
  --no-frequency-offsets \
  --no-frequency-motion \
  --single-resolution-stft-loss \
  --no-stereo-fit \
  --no-residual-noise \
  --stft-fft-sizes 2048 \
  --overwrite
```

That compatibility mode approximates the earlier fixed-bin additive pipeline:
raw FFT-bin peak frequencies, fixed requested partial count, amplitude-only
Fourier fitting, one STFT loss size, neutral stereo width, and no residual noise
bands. Residual bands remain off by default; enable them only for offline
experiments with `--residual-noise --residual-noise-bands <count>`.

## Device

Use:

```text
--device auto
```

On the current Apple Silicon Mac this resolves to PyTorch MPS. Use
`--device cpu` for deterministic debugging or when MPS behavior needs to be
isolated.

## Current Limitations

This fitter still does not optimize oscillator phase or export sampled audio.
Very dense granular or reverb-heavy drones may still need a richer model later,
but the current bank format now carries tonal partials, fitted frequency motion,
source-width metadata, and an opt-in residual noise-band layer that remains
disabled for the default VST3 soundbank.
