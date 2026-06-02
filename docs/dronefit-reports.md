# Dronefit Reports

`dronefit` can write diagnostic figures and metrics at each fitting stage.
These reports are intended to make model quality visible while the bank evolves
from spectral initialization to optimized fitting.

Generated reports are written under `reports/`, `runs/`, or `renders/` and are
ignored by git.

## Inspect a Target

```bash
uv run dronefit inspect audio/base/drones/drone_base2.wav --out reports/drone_base2_inspect --partials 96
```

Outputs:

- `metadata.json`
- `spectral_peaks.json`
- `waveform.png`
- `spectrogram_log.png`
- `features.png`
- `spectral_peaks.png`

## Initialize a Bank With Figures

```bash
uv run dronefit init-bank audio/base/drones/drone_base2.wav \
  --out runs/drone_base2_init/default.dronebank.json \
  --partials 96 \
  --report-out reports/drone_base2_init_bank
```

Outputs:

- `bank_metrics.json`
- `bank_partials.png`
- `bank_partial_distribution.png`

## Render a Bank With Figures

```bash
uv run dronefit render runs/drone_base2_init/default.dronebank.json \
  --seconds 20.211 \
  --out renders/drone_base2_init.wav \
  --report-out reports/drone_base2_init_render
```

Outputs:

- `render_metrics.json`
- `render_waveform.png`
- `render_spectrogram_log.png`
- `render_median_spectrum.png`
- `render_features.png`
- bank figures when a bank is supplied

## Compare Target and Render

```bash
uv run dronefit compare audio/base/drones/drone_base2.wav renders/drone_base2_init.wav \
  --bank runs/drone_base2_init/default.dronebank.json \
  --out reports/drone_base2_init_compare
```

Outputs:

- `comparison_metrics.json`
- `waveform_overlay.png`
- `target_spectrogram_log.png`
- `candidate_spectrogram_log.png`
- `spectrogram_delta_db.png`
- `median_spectrum_overlay.png`
- `feature_overlay.png`
- `residual_waveform.png`
- bank figures when `--bank` is supplied

## Fit Checkpoints

The PyTorch fitting loop calls `write_fit_checkpoint_report(...)` every
`--report-every` steps and once at the end of fitting.

Example:

```bash
uv run dronefit fit audio/base/drones/drone_base3.wav \
  --init-bank runs/drone_base3_init/default.dronebank.json \
  --out runs/drone_base3_fit_v001 \
  --steps 300 \
  --report-every 100 \
  --report-seconds 10
```

Outputs:

- `runs/drone_base3_fit_v001/checkpoints/step_*.dronebank.json`
- `runs/drone_base3_fit_v001/reports/step_*/`
- `runs/drone_base3_fit_v001/reports/final/`
