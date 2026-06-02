"""Command-line entry point for offline drone tools."""

from __future__ import annotations

from pathlib import Path
from typing import Annotated

import soundfile as sf
import typer
from rich.console import Console

from dronefit.audio_io import load_audio
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
    report_out: Annotated[
        Path | None,
        typer.Option(help="Optional directory for bank figures and metrics."),
    ] = None,
) -> None:
    """Create an initial additive bank from stable STFT peaks."""

    source_info = sf.info(sample)
    audio = load_audio(sample, mono=True, normalize_peak_dbfs=-6.0)
    name = out.parent.name if out.name == "default.dronebank.json" else out.stem
    bank = create_initial_bank(
        name=name,
        samples=audio.mono,
        sample_rate=audio.sample_rate,
        duration_seconds=audio.duration_seconds,
        partials=partials,
        root_midi_note=root_midi_note,
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
