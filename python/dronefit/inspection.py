"""Inspection report generation for target drone samples."""

from __future__ import annotations

import json
from pathlib import Path

import librosa
import librosa.display
import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

from dronefit.audio_io import AudioData, amplitude_to_db, load_audio
from dronefit.peaks import extract_spectral_peaks


def inspect_audio(
    path: str | Path,
    *,
    out_dir: str | Path,
    partials: int = 96,
    n_fft: int = 8192,
    hop_length: int = 512,
) -> dict[str, object]:
    """Create plots and JSON metadata for a sample."""

    audio = load_audio(path, mono=False)
    mono = audio.mono
    output_dir = Path(out_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    peaks = extract_spectral_peaks(
        mono,
        audio.sample_rate,
        partials=partials,
        n_fft=max(8192, n_fft),
        hop_length=max(512, hop_length),
    )
    metadata = _metadata(audio, peaks)

    (output_dir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    (output_dir / "spectral_peaks.json").write_text(
        json.dumps([peak.__dict__ for peak in peaks], indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    _plot_waveform(audio, mono, output_dir / "waveform.png")
    _plot_spectrogram(mono, audio.sample_rate, output_dir / "spectrogram_log.png")
    _plot_features(mono, audio.sample_rate, output_dir / "features.png", n_fft, hop_length)
    _plot_peaks(peaks, output_dir / "spectral_peaks.png")
    return metadata


def _metadata(audio: AudioData, peaks: list[object]) -> dict[str, object]:
    return {
        "path": str(audio.path),
        "sample_rate": audio.sample_rate,
        "channels": audio.channels,
        "frames": audio.frames,
        "duration_seconds": audio.duration_seconds,
        "format": audio.format,
        "subtype": audio.subtype,
        "peak": audio.peak,
        "peak_dbfs": amplitude_to_db(audio.peak),
        "rms": audio.rms,
        "rms_dbfs": audio.rms_dbfs,
        "partials_detected": len(peaks),
    }


def _plot_waveform(audio: AudioData, mono: np.ndarray, output_path: Path) -> None:
    max_points = 80_000
    step = max(1, int(np.ceil(len(mono) / max_points)))
    times = np.arange(0, len(mono), step) / audio.sample_rate
    fig, ax = plt.subplots(figsize=(12, 4), constrained_layout=True)
    ax.plot(times, mono[::step], linewidth=0.6, color="#315c8c")
    ax.set_title("Waveform")
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Amplitude")
    ax.grid(alpha=0.25)
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_spectrogram(samples: np.ndarray, sample_rate: int, output_path: Path) -> None:
    n_fft = 8192
    hop_length = 512
    stft = librosa.stft(samples, n_fft=n_fft, hop_length=hop_length, window="hann")
    db = librosa.amplitude_to_db(np.abs(stft), ref=np.max)

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
    ax.set_title("Log-Frequency Spectrogram")
    fig.colorbar(image, ax=ax, format="%+2.0f dB")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_features(
    samples: np.ndarray,
    sample_rate: int,
    output_path: Path,
    n_fft: int,
    hop_length: int,
) -> None:
    rms = librosa.feature.rms(y=samples, frame_length=n_fft, hop_length=hop_length)[0]
    centroid = librosa.feature.spectral_centroid(
        y=samples, sr=sample_rate, n_fft=n_fft, hop_length=hop_length
    )[0]
    flatness = librosa.feature.spectral_flatness(y=samples, n_fft=n_fft, hop_length=hop_length)[0]
    times = librosa.frames_to_time(np.arange(len(rms)), sr=sample_rate, hop_length=hop_length)

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
    fig.suptitle("Frame Features")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)


def _plot_peaks(peaks: list[object], output_path: Path) -> None:
    freqs = np.array([peak.freq_hz for peak in peaks], dtype=np.float64)
    amps = np.array([peak.amp_db for peak in peaks], dtype=np.float64)
    fig, ax = plt.subplots(figsize=(12, 4), constrained_layout=True)
    if len(freqs):
        ax.vlines(freqs, -80.0, amps, color="#315c8c", linewidth=0.8)
        ax.scatter(freqs, amps, color="#d6a23a", s=12)
    ax.set_xscale("log")
    ax.set_xlim(20.0, 22_000.0)
    ax.set_ylim(-80.0, 3.0)
    ax.set_title("Detected Spectral Peaks")
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Relative amplitude (dB)")
    ax.grid(alpha=0.25, which="both")
    fig.savefig(output_path, dpi=160)
    plt.close(fig)
