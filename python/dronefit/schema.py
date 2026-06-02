"""Drone bank schema and JSON serialization."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from pydantic import BaseModel, ConfigDict, Field, field_validator


class Partial(BaseModel):
    """One additive oscillator component."""

    model_config = ConfigDict(extra="forbid")

    freq_hz: float = Field(gt=0.0)
    phase_rad: float = 0.0
    amp_base: float = Field(description="Base amplitude in dB relative to the bank peak.")
    amp_coefficients: list[float] = Field(default_factory=list)
    freq_log_ratio_coefficients: list[float] = Field(default_factory=list)


class Basis(BaseModel):
    model_config = ConfigDict(extra="forbid")

    type: str = "static"
    order: int = 0
    period_seconds: float | None = None


class Macro(BaseModel):
    model_config = ConfigDict(extra="forbid")

    description: str
    default: float


class DroneBank(BaseModel):
    """Portable model asset consumed by offline tools and the future plugin."""

    model_config = ConfigDict(extra="forbid", populate_by_name=True)

    schema_id: str = Field(default="dronebank.v1", alias="schema", serialization_alias="schema")
    name: str
    created_by: str = "dronefit"
    analysis_sample_rate: int = Field(gt=0)
    render_reference_level_dbfs: float = -18.0
    model_type: str = "additive_static_peaks"
    duration_seconds: float = Field(gt=0.0)
    control_rate_hz: float = Field(default=0.0, ge=0.0)
    root_midi_note: int = Field(default=48, ge=0, le=127)
    basis: Basis = Field(default_factory=Basis)
    partials: list[Partial]
    noise_bands: list[dict[str, Any]] = Field(default_factory=list)
    macros: dict[str, Macro] = Field(default_factory=dict)
    metadata: dict[str, Any] = Field(default_factory=dict)

    @field_validator("schema_id")
    @classmethod
    def validate_schema(cls, value: str) -> str:
        if value != "dronebank.v1":
            msg = f"unsupported dronebank schema: {value}"
            raise ValueError(msg)
        return value


def load_bank(path: str | Path) -> DroneBank:
    with Path(path).open("r", encoding="utf-8") as handle:
        return DroneBank.model_validate(json.load(handle))


def save_bank(bank: DroneBank, path: str | Path) -> None:
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(bank.model_dump(mode="json", by_alias=True), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
