#!/usr/bin/env python3
"""Report local capabilities for drone fitting and VST3 development."""

from __future__ import annotations

import argparse
import importlib
import importlib.metadata
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SAMPLE = REPO_ROOT / "audio" / "base" / "drones" / "drone_base2.wav"

PYTHON_TOOLBOXES = [
    ("numpy", "numpy", True),
    ("scipy", "scipy", True),
    ("soundfile", "soundfile", True),
    ("librosa", "librosa", True),
    ("matplotlib", "matplotlib", True),
    ("torch", "torch", True),
    ("torchaudio", "torchaudio", True),
    ("typer", "typer", True),
    ("rich", "rich", True),
    ("pydantic", "pydantic", True),
    ("tqdm", "tqdm", True),
    ("pytest", "pytest", True),
    ("ruff", None, False),
    ("mypy", "mypy", False),
    ("ipykernel", "ipykernel", False),
]

SYSTEM_TOOLS = [
    ("uv", ["uv", "--version"], True),
    ("git", ["git", "--version"], True),
    ("xcode-select", ["xcode-select", "-p"], False),
    ("clang", ["clang", "--version"], False),
    ("cmake", ["cmake", "--version"], False),
    ("ninja", ["ninja", "--version"], False),
    ("pluginval", ["pluginval", "--version"], False),
]


def run_command(args: list[str], timeout: float = 8.0) -> dict[str, Any]:
    executable = shutil.which(args[0])
    if executable is None:
        return {
            "available": False,
            "path": None,
            "version": None,
            "error": "not found on PATH",
        }

    try:
        completed = subprocess.run(
            args,
            capture_output=True,
            check=False,
            text=True,
            timeout=timeout,
        )
    except Exception as exc:  # pragma: no cover - defensive system probing
        return {
            "available": True,
            "path": executable,
            "version": None,
            "error": str(exc),
        }

    output = (completed.stdout or completed.stderr).strip()
    first_line = output.splitlines()[0] if output else ""
    return {
        "available": True,
        "path": executable,
        "version": first_line,
        "error": None if completed.returncode == 0 else f"exit {completed.returncode}",
    }


def package_version(package_name: str) -> str | None:
    try:
        return importlib.metadata.version(package_name)
    except importlib.metadata.PackageNotFoundError:
        return None


def check_python_toolboxes() -> list[dict[str, Any]]:
    results = []
    for package_name, import_name, required in PYTHON_TOOLBOXES:
        version = package_version(package_name)
        importable = None
        import_error = None
        if import_name is not None and version is not None:
            try:
                importlib.import_module(import_name)
                importable = True
            except Exception as exc:  # pragma: no cover - depends on local install
                importable = False
                import_error = str(exc)

        results.append(
            {
                "name": package_name,
                "version": version,
                "installed": version is not None,
                "import_checked": import_name is not None,
                "importable": importable,
                "import_error": import_error,
                "required": required,
            }
        )
    return results


def check_torch() -> dict[str, Any]:
    version = package_version("torch")
    result: dict[str, Any] = {
        "installed": version is not None,
        "version": version,
        "cuda_available": False,
        "mps_built": False,
        "mps_available": False,
        "mps_smoke_ok": False,
        "mps_smoke_error": None,
        "recommended_device": "cpu",
    }
    if version is None:
        return result

    try:
        import torch

        result["cuda_available"] = bool(torch.cuda.is_available())
        result["mps_built"] = bool(torch.backends.mps.is_built())
        result["mps_available"] = bool(torch.backends.mps.is_available())
        if result["mps_available"]:
            tensor = torch.arange(8, dtype=torch.float32, device="mps")
            value = float((tensor * tensor).sum().cpu())
            result["mps_smoke_ok"] = value == 140.0
            result["recommended_device"] = "mps"
    except Exception as exc:  # pragma: no cover - depends on local install
        result["mps_smoke_error"] = str(exc)
    return result


def check_audio_sample(path: Path) -> dict[str, Any]:
    result: dict[str, Any] = {
        "path": str(path),
        "exists": path.exists(),
        "samplerate": None,
        "channels": None,
        "frames": None,
        "duration_seconds": None,
        "format": None,
        "subtype": None,
        "error": None,
    }
    if not path.exists():
        return result

    try:
        import soundfile as sf

        info = sf.info(path)
        result.update(
            {
                "samplerate": info.samplerate,
                "channels": info.channels,
                "frames": info.frames,
                "duration_seconds": info.frames / info.samplerate,
                "format": info.format,
                "subtype": info.subtype,
            }
        )
    except Exception as exc:  # pragma: no cover - depends on local file/libs
        result["error"] = str(exc)
    return result


def collect(sample: Path) -> dict[str, Any]:
    return {
        "repo_root": str(REPO_ROOT),
        "host": {
            "platform": platform.platform(),
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor(),
        },
        "python": {
            "version": sys.version.replace("\n", " "),
            "executable": sys.executable,
            "prefix": sys.prefix,
        },
        "python_toolboxes": check_python_toolboxes(),
        "torch_acceleration": check_torch(),
        "system_tools": [
            {"name": name, "required": required, **run_command(command)}
            for name, command, required in SYSTEM_TOOLS
        ],
        "audio_sample": check_audio_sample(sample),
    }


def status(value: bool) -> str:
    return "OK" if value else "MISSING"


def print_report(report: dict[str, Any]) -> None:
    host = report["host"]
    python = report["python"]
    print("Host")
    print(f"  Platform: {host['platform']}")
    print(f"  Machine:  {host['machine']}")
    print()

    print("Python")
    print(f"  Version:    {python['version']}")
    print(f"  Executable: {python['executable']}")
    print(f"  Prefix:     {python['prefix']}")
    print()

    print("Python toolboxes")
    for item in report["python_toolboxes"]:
        installed = item["installed"]
        importable = item["importable"]
        if item["import_checked"] and installed:
            ok = installed and importable
        else:
            ok = installed
        required = "required" if item["required"] else "optional"
        version = item["version"] or "-"
        print(f"  {status(ok):8} {item['name']:12} {version:12} {required}")
        if item["import_error"]:
            print(f"           import error: {item['import_error']}")
    print()

    torch_info = report["torch_acceleration"]
    print("PyTorch acceleration")
    print(f"  torch:              {torch_info['version'] or '-'}")
    print(f"  CUDA available:     {torch_info['cuda_available']}")
    print(f"  MPS built:          {torch_info['mps_built']}")
    print(f"  MPS available:      {torch_info['mps_available']}")
    print(f"  MPS smoke test:     {torch_info['mps_smoke_ok']}")
    print(f"  Recommended device: {torch_info['recommended_device']}")
    if torch_info["mps_smoke_error"]:
        print(f"  MPS error:          {torch_info['mps_smoke_error']}")
    print()

    print("System tools")
    for item in report["system_tools"]:
        required = "required" if item["required"] else "optional"
        version = item["version"] or "-"
        print(f"  {status(item['available']):8} {item['name']:12} {version} ({required})")
        if item["error"] and item["available"]:
            print(f"           warning: {item['error']}")
    print()

    sample = report["audio_sample"]
    print("Audio sample")
    print(f"  Path:     {sample['path']}")
    print(f"  Exists:   {sample['exists']}")
    if sample["exists"]:
        print(f"  Rate:     {sample['samplerate']} Hz")
        print(f"  Channels: {sample['channels']}")
        print(f"  Duration: {sample['duration_seconds']:.3f} s")
        print(f"  Format:   {sample['format']} / {sample['subtype']}")
    if sample["error"]:
        print(f"  Error:    {sample['error']}")


def required_failures(report: dict[str, Any]) -> list[str]:
    failures = []
    for item in report["python_toolboxes"]:
        if item["required"] and not item["installed"]:
            failures.append(f"missing required Python toolbox: {item['name']}")
        if item["required"] and item["import_checked"] and item["installed"] and not item["importable"]:
            failures.append(f"cannot import required Python toolbox: {item['name']}")
    for item in report["system_tools"]:
        if item["required"] and not item["available"]:
            failures.append(f"missing required system tool: {item['name']}")
    if not report["torch_acceleration"]["mps_available"]:
        failures.append("PyTorch MPS acceleration is not available")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check installed capabilities for drone fitting and VST3 development."
    )
    parser.add_argument(
        "--sample",
        type=Path,
        default=DEFAULT_SAMPLE,
        help=f"Audio sample to inspect. Default: {DEFAULT_SAMPLE}",
    )
    parser.add_argument("--json", action="store_true", help="Print machine-readable JSON.")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit nonzero if required capabilities are missing.",
    )
    args = parser.parse_args()

    sample = args.sample
    if not sample.is_absolute():
        sample = REPO_ROOT / sample

    report = collect(sample)
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print_report(report)

    failures = required_failures(report)
    if args.strict and failures:
        if not args.json:
            print()
            print("Strict failures")
            for failure in failures:
                print(f"  - {failure}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
