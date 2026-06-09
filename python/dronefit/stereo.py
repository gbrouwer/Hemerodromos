"""Stereo analysis helpers for fitted drone banks."""

from __future__ import annotations

import numpy as np


def estimate_stereo_width(samples: np.ndarray) -> float:
    """Estimate a compact 0..0.70 stereo-width value from source audio."""

    array = np.asarray(samples, dtype=np.float32)
    if array.ndim != 2 or array.shape[1] < 2:
        return 0.35

    left = array[:, 0].astype(np.float64)
    right = array[:, 1].astype(np.float64)
    mid = 0.5 * (left + right)
    side = 0.5 * (left - right)
    mid_rms = _rms(mid)
    side_rms = _rms(side)
    if mid_rms + side_rms <= 1.0e-9:
        return 0.35

    return float(np.clip(0.70 * side_rms / (mid_rms + side_rms), 0.0, 0.70))


def _rms(samples: np.ndarray) -> float:
    if len(samples) == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(samples), dtype=np.float64)))
