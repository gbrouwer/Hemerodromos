"""Spectral peak extraction for initial additive drone banks."""

from __future__ import annotations

from dataclasses import dataclass

import librosa
import numpy as np
from scipy.signal import find_peaks

from dronefit.audio_io import amplitude_to_db
from dronefit.schema import Basis, DroneBank, Macro, Partial


@dataclass(frozen=True)
class SpectralPeak:
    freq_hz: float
    amp_db: float
    magnitude: float
    bin_index: int


def extract_spectral_peaks(
    samples: np.ndarray,
    sample_rate: int,
    *,
    partials: int = 96,
    n_fft: int = 16384,
    hop_length: int | None = None,
    min_freq_hz: float = 20.0,
    max_freq_hz: float | None = None,
    min_spacing_hz: float = 8.0,
    prominence_db: float = 6.0,
) -> list[SpectralPeak]:
    """Find stable spectral peaks in a mono signal."""

    if samples.ndim != 1:
        msg = "extract_spectral_peaks expects a mono 1D signal"
        raise ValueError(msg)

    y = np.asarray(samples, dtype=np.float32)
    if not np.any(y):
        return []

    hop = hop_length or n_fft // 4
    fft_size = min(n_fft, max(2048, 2 ** int(np.floor(np.log2(max(len(y), 2048))))))
    stft = librosa.stft(y, n_fft=fft_size, hop_length=hop, window="hann", center=True)
    magnitude = np.median(np.abs(stft), axis=1)
    freqs = librosa.fft_frequencies(sr=sample_rate, n_fft=fft_size)

    max_freq = max_freq_hz or min(20_000.0, sample_rate * 0.48)
    valid = (freqs >= min_freq_hz) & (freqs <= max_freq)
    if not np.any(valid):
        return []

    magnitude_db = librosa.amplitude_to_db(magnitude, ref=np.max)
    peak_indices, _ = find_peaks(magnitude_db, prominence=prominence_db)
    peak_indices = [idx for idx in peak_indices if valid[idx]]

    if not peak_indices:
        sorted_bins = np.argsort(magnitude[valid])[::-1]
        valid_indices = np.flatnonzero(valid)
        peak_indices = [int(valid_indices[idx]) for idx in sorted_bins[: partials * 2]]

    peak_indices = sorted(peak_indices, key=lambda idx: magnitude[idx], reverse=True)
    selected: list[int] = []
    for idx in peak_indices:
        freq = float(freqs[idx])
        if all(abs(freq - float(freqs[other])) >= min_spacing_hz for other in selected):
            selected.append(int(idx))
        if len(selected) >= partials:
            break

    peak_mag = float(np.max(magnitude[selected], initial=1e-12))
    peaks = [
        SpectralPeak(
            freq_hz=float(freqs[idx]),
            amp_db=amplitude_to_db(float(magnitude[idx] / peak_mag)),
            magnitude=float(magnitude[idx]),
            bin_index=int(idx),
        )
        for idx in selected
    ]
    return sorted(peaks, key=lambda peak: peak.freq_hz)


def create_initial_bank(
    *,
    name: str,
    samples: np.ndarray,
    sample_rate: int,
    duration_seconds: float,
    partials: int = 96,
    root_midi_note: int = 48,
) -> DroneBank:
    peaks = extract_spectral_peaks(samples, sample_rate, partials=partials)
    return DroneBank(
        name=name,
        analysis_sample_rate=sample_rate,
        model_type="additive_static_peaks",
        duration_seconds=duration_seconds,
        control_rate_hz=0.0,
        root_midi_note=root_midi_note,
        basis=Basis(type="static", order=0, period_seconds=duration_seconds),
        partials=[
            Partial(
                freq_hz=peak.freq_hz,
                phase_rad=0.0,
                amp_base=peak.amp_db,
            )
            for peak in peaks
        ],
        macros={
            "brightness": Macro(
                description="Spectral tilt applied around the detected partial bank.",
                default=0.5,
            ),
            "motionDepth": Macro(
                description="Reserved for fitted amplitude/frequency motion controls.",
                default=0.0,
            ),
        },
        metadata={
            "initialization": "median_stft_peak_extraction",
            "partials_requested": partials,
            "partials_detected": len(peaks),
        },
    )
