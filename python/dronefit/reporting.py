"""Figure and metric reports for drone fitting checkpoints."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import librosa
import librosa.display
import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

from dronefit.audio_io import amplitude_to_db
from dronefit.schema import DroneBank


DEFAULT_N_FFT = 8192
DEFAULT_HOP_LENGTH = 512


def write_bank_report(bank: DroneBank, out_dir: str | Path) -> dict[str, Any]:
    """Write bank-level figures and metadata."""

    output_dir = Path(out_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    metrics = {
        "name": bank.name,
        "schema": bank.schema_id,
        "model_type": bank.model_type,
        "analysis_sample_rate": bank.analysis_sample_rate,
        "duration_seconds": bank.duration_seconds,
        "partials": len(bank.partials),
        "min_partial_hz": min((partial.freq_hz for partial in bank.partials), default=None),
        "max_partial_hz": max((partial.freq_hz for partial in bank.partials), default=None),
        "loudest_partial_db": max((partial.amp_base for partial in bank.partials), default=None),
        "quietest_partial_db": min((partial.amp_base for partial in bank.partials), default=None),
    }

    (output_dir / "bank_metrics.json").write_text(
        json.dumps(metrics, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    _plot_bank_partials(bank, output_dir / "bank_partials.png")
    _plot_bank_partial_distribution(bank, output_dir / "bank_partial_distribution.png")
    return metrics


def write_render_report(
    samples: np.ndarray,
    sample_rate: int,
    out_dir: str | Path,
    *,
    bank: DroneBank | None = None,
    label: str = "render",
) -> dict[str, Any]:
    """Write figures for one rendered or source signal."""

    output_dir = Path(out_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    mono = _as_mono(samples)
    metrics = {
        "label": label,
        "sample_rate": sample_rate,
        "frames": int(len(mono)),
        "duration_seconds": len(mono) / sample_rate,
        "peak": float(np.max(np.abs(mono), initial=0.0)),
        "peak_dbfs": amplitude_to_db(float(np.max(np.abs(mono), initial=0.0))),
        "rms": _rms(mono),
        "rms_dbfs": amplitude_to_db(_rms(mono)),
    }
    if bank is not None:
        metrics["bank"] = {
            "name": bank.name,
            "model_type": bank.model_type,
            "partials": len(bank.partials),
        }
        write_bank_report(bank, output_dir)

    (output_dir / "render_metrics.json").write_text(
        json.dumps(metrics, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    _plot_waveform(mono, sample_rate, output_dir / f"{label}_waveform.png", title=f"{label} waveform")
    _plot_log_spectrogram(
        mono,
        sample_rate,
        output_dir / f"{label}_spectrogram_log.png",
        title=f"{label} log-frequency spectrogram",
    )
    _plot_median_spectrum(
        mono,
        sample_rate,
        output_dir / f"{label}_median_spectrum.png",
        title=f"{label} median spectrum",
    )
    _plot_features(
        mono,
        sample_rate,
        output_dir / f"{label}_features.png",
        title=f"{label} frame features",
    )
    return metrics


def write_comparison_report(
    target_samples: np.ndarray,
    target_sample_rate: int,
    candidate_samples: np.ndarray,
    candidate_sample_rate: int,
    out_dir: str | Path,
    *,
    bank: DroneBank | None = None,
    extra_metrics: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Write target-vs-candidate figures and metrics."""

    output_dir = Path(out_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    target = _as_mono(target_samples)
    candidate = _as_mono(candidate_samples)
    if candidate_sample_rate != target_sample_rate:
        candidate = librosa.resample(
            candidate,
            orig_sr=candidate_sample_rate,
            target_sr=target_sample_rate,
        )
    sample_rate = target_sample_rate
    target, candidate = _trim_to_common_length(target, candidate)

    target_db = _stft_db(target, sample_rate)
    candidate_db = _stft_db(candidate, sample_rate)
    diff_db = candidate_db - target_db
    residual = target - candidate
    metrics = {
        "sample_rate": sample_rate,
        "frames_compared": int(len(target)),
        "duration_seconds": len(target) / sample_rate,
        "target_rms": _rms(target),
        "candidate_rms": _rms(candidate),
        "residual_rms": _rms(residual),
        "residual_rms_dbfs": amplitude_to_db(_rms(residual)),
        "mae": float(np.mean(np.abs(residual), dtype=np.float64)),
        "rmse": _rms(residual),
        "mean_abs_stft_db_delta": float(np.mean(np.abs(diff_db), dtype=np.float64)),
        "median_abs_stft_db_delta": float(np.median(np.abs(diff_db))),
        "max_abs_stft_db_delta": float(np.max(np.abs(diff_db), initial=0.0)),
    }
    if bank is not None:
        metrics["bank"] = {
            "name": bank.name,
            "model_type": bank.model_type,
            "partials": len(bank.partials),
        }
        write_bank_report(bank, output_dir)
    if extra_metrics:
        metrics["extra"] = extra_metrics

    (output_dir / "comparison_metrics.json").write_text(
        json.dumps(metrics, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    _plot_waveform_overlay(target, candidate, sample_rate, output_dir / "waveform_overlay.png")
    _plot_log_spectrogram(
        target,
        sample_rate,
        output_dir / "target_spectrogram_log.png",
        title="target log-frequency spectrogram",
    )
    _plot_log_spectrogram(
        candidate,
        sample_rate,
        output_dir / "candidate_spectrogram_log.png",
        title="candidate log-frequency spectrogram",
    )
    _plot_spectrogram_delta(diff_db, sample_rate, output_dir / "spectrogram_delta_db.png")
    _plot_spectrum_overlay(target, candidate, sample_rate, output_dir / "median_spectrum_overlay.png")
    _plot_feature_overlay(target, candidate, sample_rate, output_dir / "feature_overlay.png")
    _plot_waveform(residual, sample_rate, output_dir / "residual_waveform.png", title="residual waveform")
    return metrics


def write_fit_checkpoint_report(
    target_samples: np.ndarray,
    rendered_samples: np.ndarray,
    sample_rate: int,
    out_dir: str | Path,
    *,
    bank: DroneBank | None = None,
    step: int | None = None,
    loss: float | None = None,
    extra_metrics: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Write figures for a fitting checkpoint.

    This is intentionally independent of the future optimizer implementation,
    so the PyTorch fit loop can call it every N steps with NumPy audio.
    """

    checkpoint_metrics = dict(extra_metrics or {})
    if step is not None:
        checkpoint_metrics["step"] = step
    if loss is not None:
        checkpoint_metrics["loss"] = loss
    return write_comparison_report(
        target_samples,
        sample_rate,
        rendered_samples,
        sample_rate,
        out_dir,
        bank=bank,
        extra_metrics=checkpoint_metrics or None,
    )


def _as_mono(samples: np.ndarray) -> np.ndarray:
    array = np.asarray(samples, dtype=np.float32)
    if array.ndim == 1:
        return array
    if array.ndim == 2:
        return array.mean(axis=1, dtype=np.float32)
    msg = f"expected 1D or 2D audio array, got shape {array.shape}"
    raise ValueError(msg)


def _trim_to_common_length(left: np.ndarray, right: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    frames = min(len(left), len(right))
    return left[:frames], right[:frames]


def _rms(samples: np.ndarray) -> float:
    if len(samples) == 0:
        return 0.0
    return float(np.sqrt(np.mean(np.square(samples), dtype=np.float64)))


def _stft_db(samples: np.ndarray, sample_rate: int) -> np.ndarray:
    n_fft = min(DEFAULT_N_FFT, max(2048, 2 ** int(np.floor(np.log2(max(len(samples), 2048))))))
    hop_length = min(DEFAULT_HOP_LENGTH, max(128, n_fft // 4))
    stft = librosa.stft(samples, n_fft=n_fft, hop_length=hop_length, window="hann", center=True)
    return librosa.amplitude_to_db(np.abs(stft), ref=1.0)


def _plot_waveform(samples: np.ndarray, sample_rate: int, output_path: Path, *, title: str) -> None:
    step = max(1, int(np.ceil(len(samples) / 80_000)))
    times = np.arange(0, len(samples), step) / sample_rate
    fig, ax = plt.subplots(figsize=(12, 4), constrained_layout=True)
    ax.plot(times, samples[::step], linewidth=0.6, color="#315c8c")
    ax.set_title(title)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Amplitude")
    ax.grid(alpha=0.25)
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_waveform_overlay(
    target: np.ndarray,
    candidate: np.ndarray,
    sample_rate: int,
    output_path: Path,
) -> None:
    step = max(1, int(np.ceil(len(target) / 80_000)))
    times = np.arange(0, len(target), step) / sample_rate
    fig, ax = plt.subplots(figsize=(12, 4), constrained_layout=True)
    ax.plot(times, target[::step], linewidth=0.7, color="#315c8c", label="target")
    ax.plot(times, candidate[::step], linewidth=0.65, color="#d6a23a", alpha=0.8, label="candidate")
    ax.set_title("Waveform overlay")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Amplitude")
    ax.grid(alpha=0.25)
    ax.legend(loc="upper right")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_log_spectrogram(
    samples: np.ndarray,
    sample_rate: int,
    output_path: Path,
    *,
    title: str,
) -> None:
    db = _stft_db(samples, sample_rate)
    hop_length = min(DEFAULT_HOP_LENGTH, max(128, DEFAULT_N_FFT // 4))
    fig, ax = plt.subplots(figsize=(12, 6), constrained_layout=True)
    image = librosa.display.specshow(
        db,
        sr=sample_rate,
        hop_length=hop_length,
        x_axis="time",
        y_axis="log",
        cmap="magma",
        ax=ax,
    )
    ax.set_title(title)
    fig.colorbar(image, ax=ax, format="%+2.0f dB")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_spectrogram_delta(diff_db: np.ndarray, sample_rate: int, output_path: Path) -> None:
    hop_length = min(DEFAULT_HOP_LENGTH, max(128, DEFAULT_N_FFT // 4))
    fig, ax = plt.subplots(figsize=(12, 6), constrained_layout=True)
    image = librosa.display.specshow(
        np.clip(diff_db, -36.0, 36.0),
        sr=sample_rate,
        hop_length=hop_length,
        x_axis="time",
        y_axis="log",
        cmap="coolwarm",
        vmin=-36.0,
        vmax=36.0,
        ax=ax,
    )
    ax.set_title("Candidate minus target STFT magnitude (dB)")
    fig.colorbar(image, ax=ax, format="%+2.0f dB")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _median_spectrum(samples: np.ndarray, sample_rate: int) -> tuple[np.ndarray, np.ndarray]:
    n_fft = min(DEFAULT_N_FFT, max(2048, 2 ** int(np.floor(np.log2(max(len(samples), 2048))))))
    hop_length = min(DEFAULT_HOP_LENGTH, max(128, n_fft // 4))
    stft = librosa.stft(samples, n_fft=n_fft, hop_length=hop_length, window="hann", center=True)
    magnitude = np.median(np.abs(stft), axis=1)
    freqs = librosa.fft_frequencies(sr=sample_rate, n_fft=n_fft)
    db = librosa.amplitude_to_db(magnitude, ref=np.max)
    return freqs, db


def _plot_median_spectrum(
    samples: np.ndarray,
    sample_rate: int,
    output_path: Path,
    *,
    title: str,
) -> None:
    freqs, db = _median_spectrum(samples, sample_rate)
    fig, ax = plt.subplots(figsize=(12, 4), constrained_layout=True)
    ax.plot(freqs[1:], db[1:], color="#315c8c", linewidth=0.8)
    ax.set_xscale("log")
    ax.set_xlim(20.0, sample_rate * 0.5)
    ax.set_ylim(-90.0, 3.0)
    ax.set_title(title)
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Relative magnitude (dB)")
    ax.grid(alpha=0.25, which="both")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_spectrum_overlay(
    target: np.ndarray,
    candidate: np.ndarray,
    sample_rate: int,
    output_path: Path,
) -> None:
    target_freqs, target_db = _median_spectrum(target, sample_rate)
    candidate_freqs, candidate_db = _median_spectrum(candidate, sample_rate)
    fig, ax = plt.subplots(figsize=(12, 4), constrained_layout=True)
    ax.plot(target_freqs[1:], target_db[1:], color="#315c8c", linewidth=0.9, label="target")
    ax.plot(
        candidate_freqs[1:],
        candidate_db[1:],
        color="#d6a23a",
        linewidth=0.9,
        alpha=0.85,
        label="candidate",
    )
    ax.set_xscale("log")
    ax.set_xlim(20.0, sample_rate * 0.5)
    ax.set_ylim(-90.0, 3.0)
    ax.set_title("Median spectrum overlay")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Relative magnitude (dB)")
    ax.grid(alpha=0.25, which="both")
    ax.legend(loc="upper right")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_features(
    samples: np.ndarray,
    sample_rate: int,
    output_path: Path,
    *,
    title: str,
) -> None:
    rms, centroid, flatness, times = _features(samples, sample_rate)
    fig, axes = plt.subplots(3, 1, figsize=(12, 7), sharex=True, constrained_layout=True)
    axes[0].plot(times, 20.0 * np.log10(np.maximum(rms, 1e-8)), color="#315c8c")
    axes[0].set_ylabel("RMS (dB)")
    axes[0].grid(alpha=0.25)
    axes[1].plot(times, centroid, color="#8c4f31")
    axes[1].set_ylabel("Centroid (Hz)")
    axes[1].grid(alpha=0.25)
    axes[2].plot(times, flatness, color="#4f7c42")
    axes[2].set_ylabel("Flatness")
    axes[2].set_xlabel("Time (s)")
    axes[2].grid(alpha=0.25)
    fig.suptitle(title)
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_feature_overlay(
    target: np.ndarray,
    candidate: np.ndarray,
    sample_rate: int,
    output_path: Path,
) -> None:
    target_rms, target_centroid, target_flatness, times = _features(target, sample_rate)
    candidate_rms, candidate_centroid, candidate_flatness, candidate_times = _features(
        candidate, sample_rate
    )
    count = min(len(times), len(candidate_times))
    times = times[:count]
    fig, axes = plt.subplots(3, 1, figsize=(12, 7), sharex=True, constrained_layout=True)
    axes[0].plot(times, 20.0 * np.log10(np.maximum(target_rms[:count], 1e-8)), color="#315c8c")
    axes[0].plot(
        times,
        20.0 * np.log10(np.maximum(candidate_rms[:count], 1e-8)),
        color="#d6a23a",
        alpha=0.85,
    )
    axes[0].set_ylabel("RMS (dB)")
    axes[0].grid(alpha=0.25)
    axes[1].plot(times, target_centroid[:count], color="#315c8c", label="target")
    axes[1].plot(times, candidate_centroid[:count], color="#d6a23a", alpha=0.85, label="candidate")
    axes[1].set_ylabel("Centroid (Hz)")
    axes[1].grid(alpha=0.25)
    axes[1].legend(loc="upper right")
    axes[2].plot(times, target_flatness[:count], color="#315c8c")
    axes[2].plot(times, candidate_flatness[:count], color="#d6a23a", alpha=0.85)
    axes[2].set_ylabel("Flatness")
    axes[2].set_xlabel("Time (s)")
    axes[2].grid(alpha=0.25)
    fig.suptitle("Feature overlay")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _features(samples: np.ndarray, sample_rate: int) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    n_fft = min(DEFAULT_N_FFT, max(2048, 2 ** int(np.floor(np.log2(max(len(samples), 2048))))))
    hop_length = min(DEFAULT_HOP_LENGTH, max(128, n_fft // 4))
    rms = librosa.feature.rms(y=samples, frame_length=n_fft, hop_length=hop_length)[0]
    centroid = librosa.feature.spectral_centroid(
        y=samples, sr=sample_rate, n_fft=n_fft, hop_length=hop_length
    )[0]
    flatness = librosa.feature.spectral_flatness(y=samples, n_fft=n_fft, hop_length=hop_length)[0]
    times = librosa.frames_to_time(np.arange(len(rms)), sr=sample_rate, hop_length=hop_length)
    return rms, centroid, flatness, times


def _plot_bank_partials(bank: DroneBank, output_path: Path) -> None:
    freqs = np.array([partial.freq_hz for partial in bank.partials], dtype=np.float64)
    amps = np.array([partial.amp_base for partial in bank.partials], dtype=np.float64)
    fig, ax = plt.subplots(figsize=(12, 4), constrained_layout=True)
    if len(freqs):
        ax.vlines(freqs, -90.0, amps, color="#315c8c", linewidth=0.85)
        ax.scatter(freqs, amps, s=14, color="#d6a23a", zorder=3)
    ax.set_xscale("log")
    ax.set_xlim(20.0, max(22_000.0, bank.analysis_sample_rate * 0.5))
    ax.set_ylim(-90.0, 3.0)
    ax.set_title(f"Bank partials: {bank.name}")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Relative amplitude (dB)")
    ax.grid(alpha=0.25, which="both")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_bank_partial_distribution(bank: DroneBank, output_path: Path) -> None:
    freqs = np.array([partial.freq_hz for partial in bank.partials], dtype=np.float64)
    amps = np.array([partial.amp_base for partial in bank.partials], dtype=np.float64)
    fig, axes = plt.subplots(2, 1, figsize=(12, 6), constrained_layout=True)
    if len(freqs):
        axes[0].hist(freqs, bins=min(32, max(4, len(freqs) // 2)), color="#315c8c", alpha=0.85)
        axes[0].set_xscale("log")
        axes[1].hist(amps, bins=min(32, max(4, len(amps) // 2)), color="#8c4f31", alpha=0.85)
    axes[0].set_title("Partial frequency distribution")
    axes[0].set_xlabel("Frequency (Hz)")
    axes[0].set_ylabel("Count")
    axes[0].grid(alpha=0.25, which="both")
    axes[1].set_title("Partial amplitude distribution")
    axes[1].set_xlabel("Relative amplitude (dB)")
    axes[1].set_ylabel("Count")
    axes[1].grid(alpha=0.25)
    fig.savefig(output_path, dpi=160)
    plt.close(fig)
