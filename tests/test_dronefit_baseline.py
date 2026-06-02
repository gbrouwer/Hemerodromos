from __future__ import annotations

import numpy as np
import soundfile as sf
import torch

from dronefit.fit import FitConfig, fit_bank_to_sample, fitted_render_path
from dronefit.losses import multi_resolution_stft_loss
from dronefit.peaks import extract_spectral_peaks
from dronefit.reporting import write_comparison_report, write_render_report
from dronefit.schema import Basis, DroneBank, Partial, load_bank, save_bank
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


def test_render_bank_uses_amp_motion_coefficients():
    bank = DroneBank(
        name="moving",
        analysis_sample_rate=8_000,
        duration_seconds=1.0,
        basis=Basis(type="fourier", order=1, period_seconds=1.0),
        partials=[
            Partial(
                freq_hz=220.0,
                amp_base=-24.0,
                amp_coefficients=[18.0, 0.0],
            )
        ],
    )

    rendered = render_bank(bank, channels=1, gain_db=0.0)[:, 0]
    loud_quarter = rendered[1_900:2_100]
    quiet_quarter = rendered[5_900:6_100]

    assert np.sqrt(np.mean(loud_quarter * loud_quarter)) > np.sqrt(
        np.mean(quiet_quarter * quiet_quarter)
    )


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


def test_multi_resolution_stft_loss_is_finite():
    target = torch.sin(torch.linspace(0.0, 20.0, 2048))
    prediction = target * 0.8

    loss = multi_resolution_stft_loss(prediction, target, fft_sizes=(256, 512))

    assert torch.isfinite(loss)
    assert float(loss) > 0.0


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


def test_fit_bank_to_sample_writes_fitted_outputs(tmp_path):
    sample_rate = 8_000
    t = np.arange(sample_rate // 2, dtype=np.float32) / sample_rate
    samples = (0.4 * np.sin(2.0 * np.pi * 220.0 * t)).astype(np.float32)
    sample_path = tmp_path / "target.wav"
    sf.write(sample_path, samples, sample_rate)
    bank = DroneBank(
        name="fit_smoke",
        analysis_sample_rate=sample_rate,
        duration_seconds=0.5,
        partials=[Partial(freq_hz=220.0, amp_base=-30.0)],
    )
    initial_bank_path = tmp_path / "initial.dronebank.json"
    save_bank(bank, initial_bank_path)

    result = fit_bank_to_sample(
        sample_path,
        initial_bank_path=initial_bank_path,
        out_dir=tmp_path / "fit",
        config=FitConfig(
            steps=4,
            window_seconds=0.25,
            basis_order=0,
            learning_rate=0.05,
            device="cpu",
            stft_fft_sizes=(256, 512),
            report_every=0,
        ),
    )

    fitted_bank = load_bank(tmp_path / "fit" / "default.dronebank.json")
    assert result.render_path == tmp_path / "fit" / "fit.wav"
    assert result.render_path.exists()
    assert len(fitted_bank.partials) == 1
    assert fitted_bank.metadata["fit"]["steps"] == 4
    assert (tmp_path / "fit" / "reports" / "final" / "comparison_metrics.json").exists()


def test_fitted_render_path_uses_run_directory_name(tmp_path):
    assert fitted_render_path(tmp_path / "drone_base4_fit_v001") == (
        tmp_path / "drone_base4_fit_v001" / "drone_base4_fit_v001.wav"
    )
