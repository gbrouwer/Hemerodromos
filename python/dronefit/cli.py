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


@app.command("render")
def render_command(
    bank_path: Annotated[Path, typer.Argument(help="Input .dronebank.json path.")],
    out: Annotated[Path, typer.Option(help="Output WAV path.")],
    seconds: Annotated[float | None, typer.Option(help="Render duration in seconds.")] = None,
    sample_rate: Annotated[int | None, typer.Option(help="Override render sample rate.")] = None,
    channels: Annotated[int, typer.Option(help="Output channel count.")] = 2,
    gain_db: Annotated[float, typer.Option(help="Output gain in dB.")] = -12.0,
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
