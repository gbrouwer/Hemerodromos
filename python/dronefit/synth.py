"""Offline additive rendering for drone banks."""

from __future__ import annotations

import math

import numpy as np

from dronefit.audio_io import db_to_amplitude
from dronefit.schema import DroneBank


def render_bank(
    bank: DroneBank,
    *,
    seconds: float | None = None,
    sample_rate: int | None = None,
    channels: int = 2,
    block_size: int = 8192,
    gain_db: float = -12.0,
    stereo_width: float = 0.35,
) -> np.ndarray:
    """Render an additive approximation from a bank."""

    sr = sample_rate or bank.analysis_sample_rate
    duration = seconds or bank.duration_seconds
    frames = max(1, int(round(duration * sr)))
    channel_count = max(1, int(channels))
    output = np.zeros((frames, channel_count), dtype=np.float32)
    if not bank.partials:
        return output

    phases = np.array([partial.phase_rad for partial in bank.partials], dtype=np.float64)
    freqs = np.array([partial.freq_hz for partial in bank.partials], dtype=np.float64)
    amp_base_db = np.array([partial.amp_base for partial in bank.partials], dtype=np.float64)
    amp_coefficients = [np.asarray(partial.amp_coefficients, dtype=np.float64) for partial in bank.partials]
    gain = db_to_amplitude(gain_db)

    phase_increments = 2.0 * math.pi * freqs / sr
    active = freqs < (0.48 * sr)
    partial_indices = np.arange(len(bank.partials), dtype=np.float64)
    pan = 0.5 + 0.5 * np.sin(partial_indices * 2.399963229728653)
    pan = 0.5 + (pan - 0.5) * float(np.clip(stereo_width, 0.0, 1.0))
    left_gain = np.cos(pan * math.pi * 0.5)
    right_gain = np.sin(pan * math.pi * 0.5)

    cursor = 0
    while cursor < frames:
        count = min(block_size, frames - cursor)
        samples = np.arange(count, dtype=np.float64)
        times = (cursor + samples) / sr
        model_times = _model_times(times, bank.duration_seconds)
        mono_block = np.zeros(count, dtype=np.float64)
        stereo_block = np.zeros((count, 2), dtype=np.float64)

        for index, is_active in enumerate(active):
            if not is_active:
                continue
            amp_db = amp_base_db[index] + _evaluate_fourier(
                amp_coefficients[index],
                model_times,
                bank.duration_seconds,
            )
            amp = np.power(10.0, amp_db / 20.0)
            partial_wave = amp * np.sin(phases[index] + phase_increments[index] * samples)
            if channel_count == 1:
                mono_block += partial_wave
            else:
                stereo_block[:, 0] += partial_wave * left_gain[index]
                stereo_block[:, 1] += partial_wave * right_gain[index]
            phases[index] = (phases[index] + phase_increments[index] * count) % (2.0 * math.pi)

        if channel_count == 1:
            output[cursor : cursor + count, 0] = (mono_block * gain).astype(np.float32)
        else:
            output[cursor : cursor + count, :2] = (stereo_block * gain).astype(np.float32)
            if channel_count > 2:
                output[cursor : cursor + count, 2:] = output[cursor : cursor + count, :1]
        cursor += count

    peak = float(np.max(np.abs(output), initial=0.0))
    if peak > 0.98:
        output *= np.float32(0.98 / peak)
    return output


def _model_times(times: np.ndarray, period_seconds: float) -> np.ndarray:
    if period_seconds <= 0.0:
        return times
    return np.mod(times, period_seconds)


def _evaluate_fourier(
    coefficients: np.ndarray,
    times: np.ndarray,
    period_seconds: float,
) -> np.ndarray:
    if coefficients.size == 0 or period_seconds <= 0.0:
        return np.zeros_like(times)
    output = np.zeros_like(times)
    order = coefficients.size // 2
    for harmonic in range(1, order + 1):
        sin_coefficient = coefficients[(harmonic - 1) * 2]
        cos_coefficient = coefficients[(harmonic - 1) * 2 + 1]
        angle = 2.0 * math.pi * harmonic * times / period_seconds
        output += sin_coefficient * np.sin(angle) + cos_coefficient * np.cos(angle)
    return output
