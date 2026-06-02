"""Differentiable additive fitting for drone banks."""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import Literal

import numpy as np
import torch
from rich.console import Console
from rich.progress import BarColumn, Progress, TextColumn, TimeElapsedColumn

from dronefit.audio_io import load_audio
from dronefit.losses import coefficient_smoothness_loss, multi_resolution_stft_loss, rms_loss
from dronefit.reporting import write_fit_checkpoint_report
from dronefit.schema import Basis, DroneBank, load_bank, save_bank
from dronefit.synth import render_bank

DeviceChoice = Literal["auto", "cpu", "mps"]


@dataclass(frozen=True)
class FitConfig:
    steps: int = 300
    window_seconds: float = 1.5
    basis_order: int = 4
    learning_rate: float = 0.03
    seed: int = 0
    device: DeviceChoice = "auto"
    stft_fft_sizes: tuple[int, ...] = (512, 2048, 8192)
    time_loss_weight: float = 0.02
    rms_loss_weight: float = 0.1
    coefficient_loss_weight: float = 1.0e-4
    report_every: int = 100
    report_seconds: float = 10.0
    normalize_peak_dbfs: float = -6.0


@dataclass(frozen=True)
class FitResult:
    bank: DroneBank
    final_loss: float
    device: str
    steps: int
    output_dir: Path
    render_path: Path


def fitted_render_path(output_dir: str | Path) -> Path:
    """Return the final render path for a fit run directory."""

    path = Path(output_dir)
    stem = path.name or "render"
    return path / f"{stem}.wav"


class AdditiveFitModel(torch.nn.Module):
    """Fixed-frequency additive model with learnable amplitude motion."""

    def __init__(self, bank: DroneBank, *, basis_order: int, device: torch.device) -> None:
        super().__init__()
        self.sample_rate = int(bank.analysis_sample_rate)
        self.period_seconds = float(bank.duration_seconds)
        self.basis_order = int(basis_order)

        freq_hz = torch.tensor([partial.freq_hz for partial in bank.partials], dtype=torch.float32)
        phase_rad = torch.tensor([partial.phase_rad for partial in bank.partials], dtype=torch.float32)
        amp_base = torch.tensor([partial.amp_base for partial in bank.partials], dtype=torch.float32)
        self.register_buffer("freq_hz", freq_hz.to(device))
        self.register_buffer("phase_rad", phase_rad.to(device))
        self.base_amp_db = torch.nn.Parameter(amp_base.to(device))

        coefficient_count = max(0, self.basis_order * 2)
        initial_coefficients = torch.zeros((len(bank.partials), coefficient_count), dtype=torch.float32)
        for partial_index, partial in enumerate(bank.partials):
            existing = partial.amp_coefficients[:coefficient_count]
            if existing:
                initial_coefficients[partial_index, : len(existing)] = torch.tensor(
                    existing,
                    dtype=torch.float32,
                )
        self.amp_coefficients = torch.nn.Parameter(initial_coefficients.to(device))
        self.global_gain_db = torch.nn.Parameter(torch.tensor(0.0, dtype=torch.float32, device=device))

    def forward(self, start_seconds: float, num_samples: int) -> torch.Tensor:
        sample_indices = torch.arange(num_samples, device=self.freq_hz.device, dtype=torch.float32)
        times = start_seconds + sample_indices / float(self.sample_rate)
        model_times = torch.remainder(times, self.period_seconds)
        basis = fourier_basis(model_times, self.period_seconds, self.basis_order)
        if basis.numel() == 0:
            amp_mod_db = torch.zeros(
                (self.freq_hz.shape[0], num_samples),
                device=self.freq_hz.device,
                dtype=torch.float32,
            )
        else:
            amp_mod_db = self.amp_coefficients @ basis.T

        amp_db = self.base_amp_db[:, None] + self.global_gain_db + amp_mod_db
        amp = torch.pow(torch.tensor(10.0, device=self.freq_hz.device), amp_db / 20.0)
        phase = self.phase_rad[:, None] + 2.0 * math.pi * self.freq_hz[:, None] * times[None, :]
        active = (self.freq_hz < 0.48 * self.sample_rate).to(torch.float32)[:, None]
        return torch.sum(active * amp * torch.sin(phase), dim=0)


def fourier_basis(times: torch.Tensor, period_seconds: float, order: int) -> torch.Tensor:
    if order <= 0:
        return times.new_zeros((times.shape[0], 0))
    components = []
    for harmonic in range(1, order + 1):
        angle = 2.0 * math.pi * harmonic * times / period_seconds
        components.append(torch.sin(angle))
        components.append(torch.cos(angle))
    return torch.stack(components, dim=1)


def choose_device(choice: DeviceChoice) -> torch.device:
    if choice == "cpu":
        return torch.device("cpu")
    if choice == "mps":
        if not torch.backends.mps.is_available():
            msg = "MPS was requested but is not available"
            raise RuntimeError(msg)
        return torch.device("mps")
    if torch.backends.mps.is_available():
        return torch.device("mps")
    return torch.device("cpu")


def fit_bank_to_sample(
    sample_path: str | Path,
    *,
    initial_bank_path: str | Path,
    out_dir: str | Path,
    config: FitConfig,
    console: Console | None = None,
) -> FitResult:
    """Fit amplitude and amplitude-motion coefficients for an initialized bank."""

    output_dir = Path(out_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    report_dir = output_dir / "reports"
    checkpoint_dir = output_dir / "checkpoints"
    checkpoint_dir.mkdir(parents=True, exist_ok=True)

    bank = load_bank(initial_bank_path)
    audio = load_audio(sample_path, mono=True, normalize_peak_dbfs=config.normalize_peak_dbfs)
    if audio.sample_rate != bank.analysis_sample_rate:
        msg = (
            "sample rate mismatch between target "
            f"({audio.sample_rate}) and bank ({bank.analysis_sample_rate})"
        )
        raise ValueError(msg)

    device = choose_device(config.device)
    target = torch.tensor(audio.mono, dtype=torch.float32, device=device)
    model = AdditiveFitModel(bank, basis_order=config.basis_order, device=device)
    optimizer = torch.optim.Adam(model.parameters(), lr=config.learning_rate)
    rng = np.random.default_rng(config.seed)
    window_samples = max(2048, int(round(config.window_seconds * audio.sample_rate)))
    window_samples = min(window_samples, len(audio.mono))
    max_start = max(0, len(audio.mono) - window_samples)
    loss_history: list[dict[str, float | int]] = []
    progress_console = console or Console()

    with Progress(
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        TextColumn("{task.completed}/{task.total}"),
        TimeElapsedColumn(),
        console=progress_console,
        transient=True,
    ) as progress:
        task = progress.add_task("fitting", total=config.steps)
        for step in range(1, config.steps + 1):
            start_sample = int(rng.integers(0, max_start + 1)) if max_start > 0 else 0
            start_seconds = start_sample / audio.sample_rate
            target_window = target[start_sample : start_sample + window_samples]
            prediction = model(start_seconds, int(target_window.shape[0]))
            spectral_loss = multi_resolution_stft_loss(
                prediction,
                target_window,
                fft_sizes=config.stft_fft_sizes,
            )
            time_loss = torch.mean(torch.abs(prediction - target_window))
            energy_loss = rms_loss(prediction, target_window)
            coefficient_loss = coefficient_smoothness_loss(model.amp_coefficients)
            loss = (
                spectral_loss
                + config.time_loss_weight * time_loss
                + config.rms_loss_weight * energy_loss
                + config.coefficient_loss_weight * coefficient_loss
            )

            optimizer.zero_grad(set_to_none=True)
            loss.backward()
            optimizer.step()

            loss_record = {
                "step": step,
                "loss": float(loss.detach().cpu()),
                "spectral_loss": float(spectral_loss.detach().cpu()),
                "time_loss": float(time_loss.detach().cpu()),
                "rms_loss": float(energy_loss.detach().cpu()),
                "coefficient_loss": float(coefficient_loss.detach().cpu()),
            }
            loss_history.append(loss_record)
            if config.report_every > 0 and step % config.report_every == 0:
                checkpoint_bank = export_model_to_bank(
                    bank,
                    model,
                    config=config,
                    loss_history=loss_history,
                    model_type="additive_fourier_amp_controls",
                )
                checkpoint_path = checkpoint_dir / f"step_{step:06d}.dronebank.json"
                save_bank(checkpoint_bank, checkpoint_path)
                report_seconds = min(config.report_seconds, audio.duration_seconds)
                rendered = render_bank(
                    checkpoint_bank,
                    seconds=report_seconds,
                    sample_rate=audio.sample_rate,
                    channels=1,
                    gain_db=0.0,
                )[:, 0]
                write_fit_checkpoint_report(
                    audio.mono[: len(rendered)],
                    rendered,
                    audio.sample_rate,
                    report_dir / f"step_{step:06d}",
                    bank=checkpoint_bank,
                    step=step,
                    loss=loss_record["loss"],
                )
            progress.advance(task)

    fitted_bank = export_model_to_bank(
        bank,
        model,
        config=config,
        loss_history=loss_history,
        model_type="additive_fourier_amp_controls",
    )
    bank_path = output_dir / "default.dronebank.json"
    save_bank(fitted_bank, bank_path)

    render_path = fitted_render_path(output_dir)
    rendered = render_bank(
        fitted_bank,
        seconds=fitted_bank.duration_seconds,
        sample_rate=audio.sample_rate,
        channels=2,
        gain_db=0.0,
    )
    import soundfile as sf

    sf.write(render_path, rendered, audio.sample_rate, subtype="PCM_24")
    write_fit_checkpoint_report(
        audio.mono,
        rendered.mean(axis=1, dtype=np.float32),
        audio.sample_rate,
        report_dir / "final",
        bank=fitted_bank,
        step=config.steps,
        loss=loss_history[-1]["loss"] if loss_history else None,
    )
    return FitResult(
        bank=fitted_bank,
        final_loss=float(loss_history[-1]["loss"]) if loss_history else float("nan"),
        device=str(device),
        steps=config.steps,
        output_dir=output_dir,
        render_path=render_path,
    )


def export_model_to_bank(
    source_bank: DroneBank,
    model: AdditiveFitModel,
    *,
    config: FitConfig,
    loss_history: list[dict[str, float | int]],
    model_type: str,
) -> DroneBank:
    base_amp_db = model.base_amp_db.detach().cpu().numpy()
    global_gain_db = float(model.global_gain_db.detach().cpu())
    coefficients = model.amp_coefficients.detach().cpu().numpy()
    bank = source_bank.model_copy(deep=True)
    bank.model_type = model_type
    bank.basis = Basis(
        type="fourier" if config.basis_order > 0 else "static",
        order=config.basis_order,
        period_seconds=bank.duration_seconds,
    )
    bank.control_rate_hz = 50.0 if config.basis_order > 0 else 0.0
    for index, partial in enumerate(bank.partials):
        partial.amp_base = float(base_amp_db[index] + global_gain_db)
        partial.amp_coefficients = [float(value) for value in coefficients[index]]
    bank.metadata = dict(bank.metadata)
    bank.metadata["fit"] = {
        "method": "fixed_frequency_additive_amp_fourier",
        "steps": config.steps,
        "window_seconds": config.window_seconds,
        "basis_order": config.basis_order,
        "learning_rate": config.learning_rate,
        "device": str(next(model.parameters()).device),
        "normalize_peak_dbfs": config.normalize_peak_dbfs,
        "global_gain_db_folded_into_amp_base": global_gain_db,
        "final_loss": float(loss_history[-1]["loss"]) if loss_history else None,
        "loss_history_tail": loss_history[-10:],
    }
    return bank
