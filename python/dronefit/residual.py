"""Residual noise-band estimation for optional offline texture fitting."""

from __future__ import annotations

import numpy as np

from dronefit.audio_io import amplitude_to_db


def estimate_residual_noise_bands(
    target: np.ndarray,
    candidate: np.ndarray,
    sample_rate: int,
    *,
    bands: int,
    min_freq_hz: float = 40.0,
    max_freq_hz: float | None = None,
) -> list[dict[str, float | int]]:
    """Summarize broad-band residual energy as deterministic filtered-noise bands."""

    band_count = max(0, int(bands))
    if band_count == 0:
        return []

    frame_count = min(len(target), len(candidate))
    if frame_count == 0:
        return []

    residual = np.asarray(target[:frame_count], dtype=np.float32) - np.asarray(
        candidate[:frame_count],
        dtype=np.float32,
    )
    if not np.any(residual):
        return []

    nyquist = float(sample_rate) * 0.5
    max_freq = min(max_freq_hz or nyquist * 0.94, nyquist * 0.94)
    min_freq = max(20.0, min_freq_hz)
    if max_freq <= min_freq:
        return []

    freqs = np.fft.rfftfreq(frame_count, d=1.0 / float(sample_rate))
    spectrum = np.fft.rfft(residual)
    edges = np.geomspace(min_freq, max_freq, band_count + 1)

    noise_bands: list[dict[str, float | int]] = []
    for index, (low, high) in enumerate(zip(edges[:-1], edges[1:], strict=False)):
        mask = (freqs >= low) & (freqs < high)
        if not np.any(mask):
            continue

        band_spectrum = np.zeros_like(spectrum)
        band_spectrum[mask] = spectrum[mask]
        band_signal = np.fft.irfft(band_spectrum, n=frame_count).astype(np.float32)
        rms = float(np.sqrt(np.mean(np.square(band_signal.astype(np.float64)))))
        if rms <= 1.0e-8:
            continue

        noise_bands.append(
            {
                "min_hz": float(low),
                "max_hz": float(high),
                "gain_db": amplitude_to_db(rms),
                "seed": 9173 + index * 37,
            }
        )

    return noise_bands
