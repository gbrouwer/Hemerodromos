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
    freq_coefficients = [
        np.asarray(partial.freq_log_ratio_coefficients, dtype=np.float64) for partial in bank.partials
    ]
    gain = db_to_amplitude(gain_db)
    fitted_width = stereo_width
    has_frequency_motion = any(coefficients.size > 0 for coefficients in freq_coefficients)

    phase_increments = 2.0 * math.pi * freqs / sr
    active = freqs < (0.48 * sr)
    partial_indices = np.arange(len(bank.partials), dtype=np.float64)
    pan = 0.5 + 0.5 * np.sin(partial_indices * 2.399963229728653)
    pan = 0.5 + (pan - 0.5) * float(np.clip(fitted_width, 0.0, 1.0))
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

        for index in range(len(bank.partials)):
            if not active[index]:
                continue

            amp_db = amp_base_db[index] + _evaluate_fourier(
                amp_coefficients[index],
                model_times,
                bank.duration_seconds,
            )
            amp = np.power(10.0, amp_db / 20.0)
            if has_frequency_motion and freq_coefficients[index].size > 0:
                freq_ratio = np.power(
                    2.0,
                    _evaluate_fourier(
                        freq_coefficients[index],
                        model_times,
                        bank.duration_seconds,
                    ),
                )
                block_freq = freqs[index] * freq_ratio
                block_active = block_freq < (0.48 * sr)
                increments = 2.0 * math.pi * block_freq / sr
                partial_phase = phases[index] + np.cumsum(increments) - increments
                partial_wave = block_active * amp * np.sin(partial_phase)
                phases[index] = (phases[index] + float(np.sum(increments))) % (2.0 * math.pi)
            else:
                partial_wave = amp * np.sin(phases[index] + phase_increments[index] * samples)
                phases[index] = (phases[index] + phase_increments[index] * count) % (2.0 * math.pi)
            if channel_count == 1:
                mono_block += partial_wave
            else:
                stereo_block[:, 0] += partial_wave * left_gain[index]
                stereo_block[:, 1] += partial_wave * right_gain[index]

        if channel_count == 1:
            output[cursor : cursor + count, 0] = (mono_block * gain).astype(np.float32)
        else:
            output[cursor : cursor + count, :2] = (stereo_block * gain).astype(np.float32)
            if channel_count > 2:
                output[cursor : cursor + count, 2:] = output[cursor : cursor + count, :1]
        cursor += count

    if bank.noise_bands:
        output += _render_noise_bands(
            bank.noise_bands,
            frames=frames,
            sample_rate=sr,
            channels=channel_count,
            gain=gain,
            stereo_width=fitted_width,
        )

    peak = float(np.max(np.abs(output), initial=0.0))
    if peak > 0.98:
        output *= np.float32(0.98 / peak)
    return output


def _render_noise_bands(
    noise_bands: list[dict],
    *,
    frames: int,
    sample_rate: int,
    channels: int,
    gain: float,
    stereo_width: float,
) -> np.ndarray:
    output = np.zeros((frames, channels), dtype=np.float32)
    freqs = np.fft.rfftfreq(frames, d=1.0 / float(sample_rate))
    nyquist = 0.5 * float(sample_rate)
    width = float(np.clip(stereo_width, 0.0, 1.0))

    for index, band in enumerate(noise_bands):
        low = float(band.get("min_hz", 20.0))
        high = float(band.get("max_hz", nyquist))
        if high <= low:
            continue

        mask = (freqs >= low) & (freqs < min(high, nyquist))
        if not np.any(mask):
            continue

        rng = np.random.default_rng(int(band.get("seed", 9173 + index * 37)))
        white = rng.standard_normal(frames).astype(np.float32)
        spectrum = np.fft.rfft(white)
        filtered_spectrum = np.zeros_like(spectrum)
        filtered_spectrum[mask] = spectrum[mask]
        filtered = np.fft.irfft(filtered_spectrum, n=frames).astype(np.float32)
        rms = float(np.sqrt(np.mean(np.square(filtered.astype(np.float64)))))
        if rms <= 1.0e-8:
            continue

        filtered *= np.float32(db_to_amplitude(float(band.get("gain_db", -90.0))) * gain / rms)
        if channels == 1:
            output[:, 0] += filtered
            continue

        pan = 0.5 + 0.5 * math.sin(index * 2.399963229728653)
        pan = 0.5 + (pan - 0.5) * width
        output[:, 0] += filtered * np.float32(math.cos(pan * math.pi * 0.5))
        output[:, 1] += filtered * np.float32(math.sin(pan * math.pi * 0.5))
        if channels > 2:
            output[:, 2:] += filtered[:, None]

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
