---
title: Dronefit Fitting
weight: 40
---

The first fitting implementation is an interpretable PyTorch optimizer, not a
runtime neural network for the plugin.

It starts from an initialized `.dronebank.json`, keeps detected partial
frequencies fixed, and optimizes:

- partial base amplitudes;
- a folded global gain term;
- slow Fourier amplitude-motion coefficients per partial.

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
model_type: additive_fourier_amp_controls
basis.type: fourier
basis.order: configured --basis-order
```

Each partial keeps `freq_hz` fixed and receives optimized
`amp_coefficients`. These coefficients represent the learned motion path that
future VST3 macro controls can scale or traverse at different rates.

## Device

Use:

```text
--device auto
```

On the current Apple Silicon Mac this resolves to PyTorch MPS. Use
`--device cpu` for deterministic debugging or when MPS behavior needs to be
isolated.

## Current Limitations

This first fitter does not yet optimize partial frequencies, phase, noise-band
residuals, or stereo structure. It is intended to establish the differentiable
pipeline and produce a dynamic additive bank. The next model-quality step is to
add small frequency-motion coefficients and then residual/noise-band fitting.

