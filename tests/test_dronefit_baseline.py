from __future__ import annotations

import numpy as np

from dronefit.peaks import extract_spectral_peaks
from dronefit.reporting import write_comparison_report, write_render_report
from dronefit.schema import DroneBank, Partial, load_bank, save_bank
from dronefit.synth import render_bank


def test_bank_roundtrip(tmp_path):
    bank = DroneBank(
        name="test",
        analysis_sample_rate=48_000,
        duration_seconds=1.0,
        partials=[
            Partial(freq_hz=110.0, amp_base=-3.0),
            Partial(freq_hz=220.0, amp_base=-9.0),
        ],
    )
    path = tmp_path / "test.dronebank.json"

    save_bank(bank, path)
    loaded = load_bank(path)

    assert loaded.name == "test"
    assert len(loaded.partials) == 2
    assert loaded.partials[0].freq_hz == 110.0


def test_render_bank_shape_and_finite_values():
    bank = DroneBank(
        name="test",
        analysis_sample_rate=8_000,
        duration_seconds=0.25,
        partials=[Partial(freq_hz=220.0, amp_base=-6.0)],
    )

    rendered = render_bank(bank, channels=2)

    assert rendered.shape == (2_000, 2)
    assert np.isfinite(rendered).all()
    assert np.max(np.abs(rendered)) > 0.0


def test_extract_spectral_peaks_finds_sine_frequency():
    sample_rate = 8_000
    freq_hz = 440.0
    t = np.arange(sample_rate, dtype=np.float32) / sample_rate
    samples = 0.5 * np.sin(2.0 * np.pi * freq_hz * t)

    peaks = extract_spectral_peaks(
        samples,
        sample_rate,
        partials=4,
        n_fft=4096,
        min_freq_hz=100.0,
        max_freq_hz=1_000.0,
        min_spacing_hz=20.0,
    )

    assert peaks
    strongest = min(peaks, key=lambda peak: abs(peak.freq_hz - freq_hz))
    assert abs(strongest.freq_hz - freq_hz) < 5.0


def test_render_report_writes_figures(tmp_path):
    sample_rate = 8_000
    t = np.arange(sample_rate // 2, dtype=np.float32) / sample_rate
    samples = np.sin(2.0 * np.pi * 220.0 * t).astype(np.float32)
    bank = DroneBank(
        name="test",
        analysis_sample_rate=sample_rate,
        duration_seconds=0.5,
        partials=[Partial(freq_hz=220.0, amp_base=-6.0)],
    )

    metrics = write_render_report(samples, sample_rate, tmp_path, bank=bank)

    assert metrics["frames"] == len(samples)
    assert (tmp_path / "render_metrics.json").exists()
    assert (tmp_path / "render_waveform.png").exists()
    assert (tmp_path / "render_spectrogram_log.png").exists()
    assert (tmp_path / "render_median_spectrum.png").exists()
    assert (tmp_path / "bank_partials.png").exists()


def test_comparison_report_writes_figures(tmp_path):
    sample_rate = 8_000
    t = np.arange(sample_rate // 2, dtype=np.float32) / sample_rate
    target = np.sin(2.0 * np.pi * 220.0 * t).astype(np.float32)
    candidate = 0.8 * np.sin(2.0 * np.pi * 220.0 * t).astype(np.float32)

    metrics = write_comparison_report(target, sample_rate, candidate, sample_rate, tmp_path)

    assert metrics["frames_compared"] == len(target)
    assert metrics["residual_rms"] > 0.0
    assert (tmp_path / "comparison_metrics.json").exists()
    assert (tmp_path / "waveform_overlay.png").exists()
    assert (tmp_path / "target_spectrogram_log.png").exists()
    assert (tmp_path / "candidate_spectrogram_log.png").exists()
    assert (tmp_path / "spectrogram_delta_db.png").exists()
    assert (tmp_path / "median_spectrum_overlay.png").exists()
