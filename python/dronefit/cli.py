"""Command-line entry point for offline drone tools."""

from __future__ import annotations

from pathlib import Path
import re
import shutil
from typing import Annotated

import soundfile as sf
import typer
from rich.console import Console

from dronefit.audio_io import load_audio
from dronefit.fit import FitConfig, fit_bank_to_sample, fitted_render_path
from dronefit.inspection import inspect_audio
from dronefit.peaks import create_initial_bank
from dronefit.reporting import write_bank_report, write_comparison_report, write_render_report
from dronefit.schema import load_bank, save_bank
from dronefit.synth import render_bank

app = typer.Typer(no_args_is_help=True)
console = Console()


@app.command("inspect")
def inspect_command(
    sample: Annotated[Path, typer.Argument(help="Target WAV/AIFF sample to inspect.")],
    out: Annotated[Path, typer.Option(help="Output report directory.")],
    partials: Annotated[int, typer.Option(help="Number of spectral peaks to report.")] = 96,
) -> None:
    """Generate plots and metadata for a target drone sample."""

    metadata = inspect_audio(sample, out_dir=out, partials=partials)
    console.print(f"[green]Inspection written:[/green] {out}")
    console.print(
        "sample_rate={sample_rate}Hz channels={channels} duration={duration_seconds:.3f}s "
        "partials={partials_detected}".format(**metadata)
    )


@app.command("init-bank")
def init_bank_command(
    sample: Annotated[Path, typer.Argument(help="Target WAV/AIFF sample.")],
    out: Annotated[Path, typer.Option(help="Output .dronebank.json path.")],
    partials: Annotated[int, typer.Option(help="Number of partials to initialize.")] = 96,
    root_midi_note: Annotated[int, typer.Option(help="Reference note for later transposition.")] = 48,
    sub_bin_peaks: Annotated[
        bool,
        typer.Option("--sub-bin-peaks/--no-sub-bin-peaks", help="Use sub-bin peak interpolation."),
    ] = False,
    adaptive_partial_count: Annotated[
        bool,
        typer.Option(
            "--adaptive-partial-count/--fixed-partial-count",
            help="Drop weak partials using the newer adaptive detector.",
        ),
    ] = False,
    report_out: Annotated[
        Path | None,
        typer.Option(help="Optional directory for bank figures and metrics."),
    ] = None,
) -> None:
    """Create an initial additive bank from stable STFT peaks."""

    source_info = sf.info(sample)
    audio = load_audio(sample, mono=True, normalize_peak_dbfs=-6.0)
    name = display_name_from_stem(sample.stem)
    bank = create_initial_bank(
        name=name,
        samples=audio.mono,
        sample_rate=audio.sample_rate,
        duration_seconds=audio.duration_seconds,
        partials=partials,
        root_midi_note=root_midi_note,
        use_sub_bin_interpolation=sub_bin_peaks,
        adaptive_partial_count=adaptive_partial_count,
    )
    bank.metadata.update(
        {
            "source_path": str(sample),
            "source_channels": source_info.channels,
            "source_format": source_info.format,
            "source_subtype": source_info.subtype,
            "preprocess": {
                "mono": True,
                "remove_dc": True,
                "normalize_peak_dbfs": -6.0,
            },
        }
    )
    save_bank(bank, out)
    console.print(f"[green]Bank written:[/green] {out}")
    console.print(f"partials={len(bank.partials)} sample_rate={bank.analysis_sample_rate}Hz")
    if report_out is not None:
        write_bank_report(bank, report_out)
        console.print(f"[green]Bank report written:[/green] {report_out}")


@app.command("render")
def render_command(
    bank_path: Annotated[Path, typer.Argument(help="Input .dronebank.json path.")],
    out: Annotated[Path, typer.Option(help="Output WAV path.")],
    seconds: Annotated[float | None, typer.Option(help="Render duration in seconds.")] = None,
    sample_rate: Annotated[int | None, typer.Option(help="Override render sample rate.")] = None,
    channels: Annotated[int, typer.Option(help="Output channel count.")] = 2,
    gain_db: Annotated[float, typer.Option(help="Output gain in dB.")] = -12.0,
    report_out: Annotated[
        Path | None,
        typer.Option(help="Optional directory for render figures and metrics."),
    ] = None,
) -> None:
    """Render a bank to audio without the original sample."""

    bank = load_bank(bank_path)
    rendered = render_bank(
        bank,
        seconds=seconds,
        sample_rate=sample_rate,
        channels=channels,
        gain_db=gain_db,
    )
    out.parent.mkdir(parents=True, exist_ok=True)
    sf.write(out, rendered, sample_rate or bank.analysis_sample_rate, subtype="PCM_24")
    console.print(f"[green]Render written:[/green] {out}")
    console.print(
        f"frames={len(rendered)} channels={rendered.shape[1]} "
        f"sample_rate={sample_rate or bank.analysis_sample_rate}Hz"
    )
    if report_out is not None:
        write_render_report(
            rendered,
            sample_rate or bank.analysis_sample_rate,
            report_out,
            bank=bank,
            label="render",
        )
        console.print(f"[green]Render report written:[/green] {report_out}")


@app.command("compare")
def compare_command(
    target: Annotated[Path, typer.Argument(help="Target sample WAV/AIFF.")],
    candidate: Annotated[Path, typer.Argument(help="Rendered candidate WAV/AIFF.")],
    out: Annotated[Path, typer.Option(help="Output comparison report directory.")],
    bank: Annotated[
        Path | None,
        typer.Option(help="Optional bank JSON to include bank figures in the report."),
    ] = None,
) -> None:
    """Generate target-vs-render figures and metrics."""

    target_audio = load_audio(target, mono=False, remove_dc=False)
    candidate_audio = load_audio(candidate, mono=False, remove_dc=False)
    bank_model = load_bank(bank) if bank is not None else None
    metrics = write_comparison_report(
        target_audio.samples,
        target_audio.sample_rate,
        candidate_audio.samples,
        candidate_audio.sample_rate,
        out,
        bank=bank_model,
    )
    console.print(f"[green]Comparison report written:[/green] {out}")
    console.print(
        "duration={duration_seconds:.3f}s residual_rms={residual_rms:.6f} "
        "mean_abs_stft_db_delta={mean_abs_stft_db_delta:.3f}".format(**metrics)
    )


@app.command("fit")
def fit_command(
    sample: Annotated[Path, typer.Argument(help="Target WAV/AIFF sample.")],
    out: Annotated[Path, typer.Option(help="Output directory for fitted bank, render, and reports.")],
    init_bank: Annotated[
        Path | None,
        typer.Option(help="Optional initial .dronebank.json. If omitted, one is created."),
    ] = None,
    partials: Annotated[int, typer.Option(help="Partials to use when creating an initial bank.")] = 96,
    steps: Annotated[int, typer.Option(help="Optimization steps.")] = 300,
    window_seconds: Annotated[float, typer.Option(help="Random crop length per optimization step.")] = 1.5,
    basis_order: Annotated[int, typer.Option(help="Fourier amplitude-motion order.")] = 4,
    learning_rate: Annotated[float, typer.Option(help="Adam learning rate.")] = 0.03,
    max_frequency_offset_cents: Annotated[
        float,
        typer.Option(help="Maximum learned static frequency offset per partial, in cents."),
    ] = 20.0,
    max_frequency_motion_cents: Annotated[
        float,
        typer.Option(help="Maximum learned Fourier frequency motion per partial, in cents."),
    ] = 12.0,
    residual_noise_bands: Annotated[
        int,
        typer.Option(help="Number of residual filtered-noise bands to export when --residual-noise is set."),
    ] = 0,
    sub_bin_peaks: Annotated[
        bool,
        typer.Option("--sub-bin-peaks/--no-sub-bin-peaks", help="Use sub-bin peak interpolation."),
    ] = False,
    adaptive_partial_count: Annotated[
        bool,
        typer.Option(
            "--adaptive-partial-count/--fixed-partial-count",
            help="Drop weak partials using the newer adaptive detector.",
        ),
    ] = False,
    frequency_offsets: Annotated[
        bool,
        typer.Option("--frequency-offsets/--no-frequency-offsets", help="Fit static per-partial frequency offsets."),
    ] = False,
    frequency_motion: Annotated[
        bool,
        typer.Option("--frequency-motion/--no-frequency-motion", help="Fit slow per-partial frequency motion."),
    ] = False,
    multi_resolution_stft_loss: Annotated[
        bool,
        typer.Option(
            "--multi-resolution-stft-loss/--single-resolution-stft-loss",
            help="Use all configured STFT FFT sizes instead of only the largest one.",
        ),
    ] = True,
    stereo_fit: Annotated[
        bool,
        typer.Option("--stereo-fit/--no-stereo-fit", help="Estimate fitted stereo width from the source sample."),
    ] = False,
    residual_noise: Annotated[
        bool,
        typer.Option("--residual-noise/--no-residual-noise", help="Export residual filtered-noise bands."),
    ] = False,
    device: Annotated[str, typer.Option(help="Device: auto, cpu, or mps.")] = "auto",
    report_every: Annotated[int, typer.Option(help="Write checkpoint reports every N steps; 0 disables.")] = 100,
    report_seconds: Annotated[float, typer.Option(help="Seconds rendered for checkpoint reports.")] = 10.0,
    seed: Annotated[int, typer.Option(help="Random seed for crop selection.")] = 0,
    stft_fft_sizes: Annotated[
        str,
        typer.Option(help="Comma-separated STFT FFT sizes for spectral loss."),
    ] = "512,2048,8192",
    overwrite: Annotated[
        bool,
        typer.Option(help="Allow reusing an existing run directory and overwrite known fit outputs."),
    ] = False,
) -> None:
    """Fit an initialized additive bank with PyTorch."""

    if device not in {"auto", "cpu", "mps"}:
        msg = "device must be one of: auto, cpu, mps"
        raise typer.BadParameter(msg)

    _prepare_fit_output_dir(out, overwrite=overwrite)
    initial_bank_path = init_bank
    if initial_bank_path is None:
        source_info = sf.info(sample)
        audio = load_audio(sample, mono=True, normalize_peak_dbfs=-6.0)
        initial_bank_path = out / "initial.dronebank.json"
        initial_bank = create_initial_bank(
            name=display_name_from_stem(sample.stem),
            samples=audio.mono,
            sample_rate=audio.sample_rate,
            duration_seconds=audio.duration_seconds,
            partials=partials,
            use_sub_bin_interpolation=sub_bin_peaks,
            adaptive_partial_count=adaptive_partial_count,
        )
        initial_bank.metadata.update(
            {
                "source_path": str(sample),
                "source_channels": source_info.channels,
                "source_format": source_info.format,
                "source_subtype": source_info.subtype,
                "preprocess": {
                    "mono": True,
                    "remove_dc": True,
                    "normalize_peak_dbfs": -6.0,
                },
            }
        )
        save_bank(initial_bank, initial_bank_path)
        write_bank_report(initial_bank, out / "reports" / "initial_bank")
        console.print(f"[green]Initial bank written:[/green] {initial_bank_path}")

    config = FitConfig(
        steps=steps,
        window_seconds=window_seconds,
        basis_order=basis_order,
        learning_rate=learning_rate,
        max_frequency_offset_cents=max_frequency_offset_cents,
        max_frequency_motion_cents=max_frequency_motion_cents,
        fit_frequency_offsets=frequency_offsets,
        fit_frequency_motion=frequency_motion,
        use_multi_resolution_stft_loss=multi_resolution_stft_loss,
        fit_stereo_width=stereo_fit,
        fit_residual_noise=residual_noise,
        residual_noise_bands=residual_noise_bands,
        seed=seed,
        device=device,  # type: ignore[arg-type]
        stft_fft_sizes=_parse_fft_sizes(stft_fft_sizes),
        report_every=report_every,
        report_seconds=report_seconds,
    )
    result = fit_bank_to_sample(
        sample,
        initial_bank_path=initial_bank_path,
        out_dir=out,
        config=config,
        console=console,
    )
    console.print(f"[green]Fitted bank written:[/green] {out / 'default.dronebank.json'}")
    console.print(f"[green]Fitted render written:[/green] {result.render_path}")
    console.print(f"device={result.device} steps={result.steps} final_loss={result.final_loss:.6f}")


def _parse_fft_sizes(value: str) -> tuple[int, ...]:
    sizes = tuple(int(part.strip()) for part in value.split(",") if part.strip())
    if not sizes:
        msg = "at least one FFT size is required"
        raise typer.BadParameter(msg)
    return sizes


def display_name_from_stem(stem: str) -> str:
    match = re.fullmatch(r"drone_base0*(\d+)", stem)
    if match:
        return f"Drone {int(match.group(1))}"
    return " ".join(part for part in re.split(r"[_\\-\\s]+", stem.strip()) if part).title() or "Drone"


def _prepare_fit_output_dir(out: Path, *, overwrite: bool) -> None:
    if out.exists() and not out.is_dir():
        msg = f"fit output path exists and is not a directory: {out}"
        raise typer.BadParameter(msg)

    if out.exists():
        entries = [path for path in out.iterdir() if path.name != ".DS_Store"]
        if entries and not overwrite:
            preview = ", ".join(path.name for path in sorted(entries)[:5])
            if len(entries) > 5:
                preview += ", ..."
            msg = (
                f"fit output directory is not empty: {out}. "
                f"Existing entries: {preview}. "
                "Use a new --out directory or pass --overwrite."
            )
            raise typer.BadParameter(msg)
        if overwrite:
            _remove_known_fit_outputs(out)

    out.mkdir(parents=True, exist_ok=True)


def _remove_known_fit_outputs(out: Path) -> None:
    for path in [
        out / "initial.dronebank.json",
        out / "default.dronebank.json",
        out / "render.wav",
        fitted_render_path(out),
    ]:
        if path.exists() and path.is_file():
            path.unlink()
    for path in [out / "checkpoints", out / "reports"]:
        if path.exists() and path.is_dir():
            shutil.rmtree(path)
