"""Audio loading and normalization helpers."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import numpy as np
import soundfile as sf


@dataclass(frozen=True)
class AudioData:
    """Loaded audio and source metadata."""

    path: Path
    samples: np.ndarray
    sample_rate: int
    format: str
    subtype: str

    @property
    def channels(self) -> int:
        return int(self.samples.shape[1])

    @property
    def frames(self) -> int:
        return int(self.samples.shape[0])

    @property
    def duration_seconds(self) -> float:
        return self.frames / self.sample_rate

    @property
    def mono(self) -> np.ndarray:
        return self.samples.mean(axis=1, dtype=np.float32)

    @property
    def peak(self) -> float:
        return float(np.max(np.abs(self.samples), initial=0.0))

    @property
    def rms(self) -> float:
        return float(np.sqrt(np.mean(np.square(self.samples), dtype=np.float64)))

    @property
    def rms_dbfs(self) -> float:
        return amplitude_to_db(self.rms)


def amplitude_to_db(value: float, floor_db: float = -120.0) -> float:
    if value <= 0.0:
        return floor_db
    return max(floor_db, 20.0 * float(np.log10(value)))


def db_to_amplitude(value_db: float) -> float:
    return float(10.0 ** (value_db / 20.0))


def load_audio(
    path: str | Path,
    *,
    mono: bool = False,
    remove_dc: bool = True,
    normalize_peak_dbfs: float | None = None,
) -> AudioData:
    """Load audio as float32 in the conventional -1..1 range."""

    audio_path = Path(path)
    samples, sample_rate = sf.read(audio_path, dtype="float32", always_2d=True)
    if remove_dc and samples.size:
        samples = samples - samples.mean(axis=0, keepdims=True, dtype=np.float64).astype(np.float32)
    if mono and samples.shape[1] > 1:
        samples = samples.mean(axis=1, keepdims=True, dtype=np.float32)

    if normalize_peak_dbfs is not None:
        peak = float(np.max(np.abs(samples), initial=0.0))
        if peak > 0.0:
            samples = samples * (db_to_amplitude(normalize_peak_dbfs) / peak)

    info = sf.info(audio_path)
    return AudioData(
        path=audio_path,
        samples=np.asarray(samples, dtype=np.float32),
        sample_rate=int(sample_rate),
        format=info.format,
        subtype=info.subtype,
    )
