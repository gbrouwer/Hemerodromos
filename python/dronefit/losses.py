"""PyTorch losses for differentiable drone fitting."""

from __future__ import annotations

import warnings

import torch


def multi_resolution_stft_loss(
    prediction: torch.Tensor,
    target: torch.Tensor,
    *,
    fft_sizes: tuple[int, ...] = (512, 2048, 8192),
    eps: float = 1.0e-7,
) -> torch.Tensor:
    """Multi-resolution magnitude STFT loss.

    Combines spectral convergence with log-magnitude L1. This avoids requiring
    exact waveform phase alignment while still pushing the model toward the
    target's spectral shape.
    """

    losses = []
    for fft_size in fft_sizes:
        if prediction.shape[-1] < fft_size or target.shape[-1] < fft_size:
            continue
        hop_length = max(1, fft_size // 4)
        window = torch.hann_window(fft_size, device=prediction.device, dtype=prediction.dtype)
        with warnings.catch_warnings():
            warnings.filterwarnings(
                "ignore",
                message="An output with one or more elements was resized.*",
                category=UserWarning,
            )
            pred_stft = torch.stft(
                prediction,
                n_fft=fft_size,
                hop_length=hop_length,
                win_length=fft_size,
                window=window,
                center=True,
                return_complex=True,
            )
            target_stft = torch.stft(
                target,
                n_fft=fft_size,
                hop_length=hop_length,
                win_length=fft_size,
                window=window,
                center=True,
                return_complex=True,
            )
        pred_mag = torch.abs(pred_stft)
        target_mag = torch.abs(target_stft)
        spectral_convergence = torch.linalg.vector_norm(target_mag - pred_mag) / (
            torch.linalg.vector_norm(target_mag) + eps
        )
        log_mag = torch.mean(torch.abs(torch.log(pred_mag + eps) - torch.log(target_mag + eps)))
        losses.append(spectral_convergence + log_mag)

    if not losses:
        return torch.mean(torch.abs(prediction - target))
    return torch.stack(losses).mean()


def rms_loss(prediction: torch.Tensor, target: torch.Tensor, *, eps: float = 1.0e-8) -> torch.Tensor:
    pred_rms = torch.sqrt(torch.mean(prediction.square()) + eps)
    target_rms = torch.sqrt(torch.mean(target.square()) + eps)
    return torch.abs(torch.log(pred_rms + eps) - torch.log(target_rms + eps))


def coefficient_smoothness_loss(coefficients: torch.Tensor) -> torch.Tensor:
    """Penalize large high-order Fourier motion coefficients."""

    if coefficients.numel() == 0:
        return coefficients.new_tensor(0.0)
    coefficient_count = coefficients.shape[-1]
    if coefficient_count == 0:
        return coefficients.new_tensor(0.0)
    order_indices = torch.arange(coefficient_count, device=coefficients.device, dtype=coefficients.dtype)
    harmonic = torch.floor(order_indices / 2.0) + 1.0
    weights = harmonic.square()
    return torch.mean(coefficients.square() * weights)
