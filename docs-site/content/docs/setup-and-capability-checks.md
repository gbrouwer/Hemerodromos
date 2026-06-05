---
title: Setup And Capability Checks
weight: 20
---

This project uses `uv` for the Python environment used by the offline drone
analysis and fitting tools.

## Host

The initial setup was verified on:

- macOS 15.7.3
- Apple Silicon / arm64
- Python 3.12.11
- uv 0.11.18

For GPU acceleration on this Mac, use PyTorch's Metal Performance Shaders
backend, usually reported as `mps`. CUDA and ROCm are not the target backends
for this machine.

## Create or Sync the Environment

From the repository root:

```bash
uv sync
```

This creates or updates `.venv` from `pyproject.toml` and `uv.lock`.

The project pins Python through:

```text
.python-version
```

Current value:

```text
3.12
```

## Installed Python Toolboxes

Runtime dependencies:

- `numpy`
- `scipy`
- `soundfile`
- `librosa`
- `matplotlib`
- `torch`
- `torchaudio`
- `typer`
- `rich`
- `pydantic`
- `tqdm`

Development dependencies:

- `pytest`
- `ruff`
- `mypy`
- `ipykernel`

To add or refresh `pytest`:

```bash
uv add --dev pytest
```

To confirm `pytest`:

```bash
uv run pytest --version
```

## Capability Checker

The environment checker is:

```text
scripts/check_capabilities.py
```

Run the normal report:

```bash
uv run python scripts/check_capabilities.py
```

Run strict mode:

```bash
uv run python scripts/check_capabilities.py --strict
```

Print JSON for automation:

```bash
uv run python scripts/check_capabilities.py --json
```

Check a different sample:

```bash
uv run python scripts/check_capabilities.py --sample audio/base/drones/drone_base2.wav
```

The checker reports:

- host OS and CPU architecture;
- active Python executable and virtual environment path;
- installed/importable Python packages;
- PyTorch CUDA and MPS status;
- a small MPS tensor smoke test;
- system tools such as `uv`, `git`, `xcode-select`, `clang`, `cmake`, `ninja`, and `pluginval`;
- audio sample metadata through `soundfile`.

## Current Verified Result

The current environment reports:

```text
Python: 3.12.11
PyTorch: 2.12.0
torchaudio: 2.11.0
CUDA available: False
MPS built: True
MPS available: True
MPS smoke test: True
Recommended device: mps
```

`pluginval` is currently missing. It is optional during Python setup, but should
be installed before validating the VST3 plugin.

## Audio Sample

The checked sample is:

```text
/Users/gbrouwer/Core/Hemerodromos/audio/base/drones/drone_base2.wav
```

Current metadata:

```text
Sample rate: 44100 Hz
Channels: 2
Duration: 20.211 s
Format: WAV / PCM_24
```

Raw audio files are ignored by git.

## Validation Commands

Run these before starting fitting work:

```bash
uv run ruff check scripts/check_capabilities.py
uv run python scripts/check_capabilities.py --strict
```

Expected result:

```text
ruff: all checks passed
capability checker: exits with code 0
```

If copying command snippets into interactive `zsh`, avoid copying expected
output lines as commands. Interactive `zsh` may treat a pasted line beginning
with `#` as a command unless `interactive_comments` is enabled.

