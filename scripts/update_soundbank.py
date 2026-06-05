#!/usr/bin/env python3
"""Fit new drone samples and rebuild/reinstall the Hemerodromos VST3s.

The source audio tree under audio/base is treated as immutable input. This
script only reads from it; cleanup is restricted to generated derivative paths.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import platform
import re
import shutil
import subprocess
from collections.abc import Sequence


AUDIO_EXTENSIONS = {".wav", ".aif", ".aiff", ".flac"}
DEFAULT_SAMPLE_ROOT = Path("audio/base")
DEFAULT_FIT_VERSION = "v001"
DEFAULT_VST3_INSTALL_DIR = Path("~/Library/Audio/Plug-Ins/VST3").expanduser()
VST3_BUILD_PRODUCTS = (
    (
        Path("build/HemerodromosDrone_artefacts/RelWithDebInfo/VST3/Hemerodromos Drone.vst3"),
        "Hemerodromos Drone.vst3",
    ),
    (
        Path(
            "build/HemerodromosDroneTriggered_artefacts/RelWithDebInfo/VST3/"
            "Hemerodromos Drone Triggered.vst3"
        ),
        "Hemerodromos Drone Triggered.vst3",
    ),
)


@dataclass(frozen=True)
class SamplePlan:
    sample: Path
    stem: str
    bank_asset: Path
    run_dir: Path
    processed: bool


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    project_root = Path(__file__).resolve().parents[1]
    sample_root = (project_root / args.sample_root).resolve()
    immutable_root = (project_root / args.immutable_root).resolve()

    if not sample_root.exists():
        raise SystemExit(f"sample root does not exist: {sample_root}")
    if not is_relative_to(sample_root, immutable_root):
        raise SystemExit(
            f"sample root must be inside immutable audio root {immutable_root}: {sample_root}"
        )

    all_plans = discover_sample_plans(
        project_root=project_root,
        sample_root=sample_root,
        fit_version=args.fit_version,
    )
    selected = select_plans(all_plans, mode=args.mode, samples=args.samples)

    print_status(all_plans, selected)
    if args.mode == "status":
        return 0

    if not selected:
        print("No samples selected for processing.")
        if args.force_build:
            build_and_install(args, project_root)
        return 0

    if needs_confirmation(selected, args) and not confirm_reprocess(selected):
        raise SystemExit("Aborted.")

    for plan in selected:
        process_sample(plan, args, project_root, immutable_root)

    if not args.skip_build:
        build_and_install(args, project_root)

    return 0


def parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Update fitted drone banks from audio/base samples, then rebuild and reinstall "
            "the Hemerodromos VST3 plugins."
        )
    )
    parser.add_argument(
        "--mode",
        choices=("new", "all", "selected", "status"),
        default="new",
        help=(
            "new: fit samples missing an asset bank; all: reprocess every sample; "
            "selected: process --samples; status: inspect only."
        ),
    )
    parser.add_argument(
        "--samples",
        nargs="*",
        default=(),
        help=(
            "Sample stems, filenames, or paths for --mode selected, e.g. "
            "drone_base13 drone_base14.wav audio/base/drones/drone_base15.wav."
        ),
    )
    parser.add_argument("--sample-root", type=Path, default=DEFAULT_SAMPLE_ROOT)
    parser.add_argument("--immutable-root", type=Path, default=DEFAULT_SAMPLE_ROOT)
    parser.add_argument("--fit-version", default=DEFAULT_FIT_VERSION)
    parser.add_argument("--partials", type=int, default=96)
    parser.add_argument("--steps", type=int, default=300)
    parser.add_argument("--window-seconds", type=float, default=1.5)
    parser.add_argument("--basis-order", type=int, default=4)
    parser.add_argument("--learning-rate", type=float, default=0.03)
    parser.add_argument("--device", choices=("auto", "cpu", "mps"), default="auto")
    parser.add_argument("--report-every", type=int, default=100)
    parser.add_argument("--report-seconds", type=float, default=10.0)
    parser.add_argument("--stft-fft-sizes", default="512,2048,8192")
    parser.add_argument("--uv-bin", default="uv")
    parser.add_argument("--build-dir", type=Path, default=Path("build"))
    parser.add_argument("--install-dir", type=Path, default=DEFAULT_VST3_INSTALL_DIR)
    parser.add_argument("--dry-run", action="store_true", help="Print actions without mutating files.")
    parser.add_argument("--yes", action="store_true", help="Skip confirmation for destructive modes.")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--skip-install", action="store_true")
    parser.add_argument("--skip-sign", action="store_true")
    parser.add_argument(
        "--force-build",
        action="store_true",
        help="Build/install even if no samples need fitting.",
    )
    return parser.parse_args(argv)


def discover_sample_plans(
    *,
    project_root: Path,
    sample_root: Path,
    fit_version: str,
) -> list[SamplePlan]:
    samples = sorted(
        (
            path
            for path in sample_root.rglob("*")
            if path.is_file() and path.suffix.lower() in AUDIO_EXTENSIONS
        ),
        key=sample_sort_key,
    )
    seen: set[str] = set()
    plans: list[SamplePlan] = []
    for sample in samples:
        stem = sample.stem
        if stem in seen:
            raise SystemExit(f"duplicate sample stem under {sample_root}: {stem}")
        seen.add(stem)
        bank_asset = project_root / "assets" / "fitted" / f"{stem}_fit_{fit_version}.dronebank.json"
        run_dir = project_root / "runs" / f"{stem}_fit_{fit_version}"
        plans.append(
            SamplePlan(
                sample=sample,
                stem=stem,
                bank_asset=bank_asset,
                run_dir=run_dir,
                processed=bank_asset.exists(),
            )
        )
    return plans


def select_plans(plans: Sequence[SamplePlan], *, mode: str, samples: Sequence[str]) -> list[SamplePlan]:
    if mode == "status":
        return []
    if mode == "new":
        return [plan for plan in plans if not plan.processed]
    if mode == "all":
        return list(plans)
    if mode == "selected":
        if not samples:
            raise SystemExit("--mode selected requires --samples")
        wanted = {normalize_sample_selector(sample) for sample in samples}
        by_stem = {plan.stem: plan for plan in plans}
        missing = sorted(wanted - set(by_stem))
        if missing:
            raise SystemExit(f"selected samples were not found under the sample root: {', '.join(missing)}")
        return [by_stem[stem] for stem in sorted(wanted, key=sample_stem_sort_key)]
    raise SystemExit(f"unknown mode: {mode}")


def print_status(all_plans: Sequence[SamplePlan], selected: Sequence[SamplePlan]) -> None:
    processed = sum(1 for plan in all_plans if plan.processed)
    print(f"Discovered {len(all_plans)} sample(s); {processed} already have fitted bank assets.")
    for plan in all_plans:
        state = "processed" if plan.processed else "new"
        marker = "*" if plan in selected else " "
        print(f"{marker} {plan.stem:<24} {state:<10} {plan.sample}")
    if selected:
        print(f"Selected {len(selected)} sample(s) for processing.")


def needs_confirmation(selected: Sequence[SamplePlan], args: argparse.Namespace) -> bool:
    if args.dry_run or args.yes:
        return False
    return any(plan.processed for plan in selected)


def confirm_reprocess(selected: Sequence[SamplePlan]) -> bool:
    existing = [plan.stem for plan in selected if plan.processed]
    print("The following processed sample(s) will have generated artifacts replaced:")
    print(", ".join(existing))
    response = input("Continue? [y/N] ").strip().lower()
    return response in {"y", "yes"}


def process_sample(
    plan: SamplePlan,
    args: argparse.Namespace,
    project_root: Path,
    immutable_root: Path,
) -> None:
    print(f"\nProcessing {plan.stem}")
    clean_derivatives(plan, project_root=project_root, immutable_root=immutable_root, dry_run=args.dry_run)

    fit_command = [
        args.uv_bin,
        "run",
        "dronefit",
        "fit",
        str(plan.sample),
        "--out",
        str(plan.run_dir),
        "--partials",
        str(args.partials),
        "--steps",
        str(args.steps),
        "--window-seconds",
        str(args.window_seconds),
        "--basis-order",
        str(args.basis_order),
        "--learning-rate",
        str(args.learning_rate),
        "--device",
        args.device,
        "--report-every",
        str(args.report_every),
        "--report-seconds",
        str(args.report_seconds),
        "--stft-fft-sizes",
        args.stft_fft_sizes,
        "--overwrite",
    ]
    run(fit_command, dry_run=args.dry_run)

    source_bank = plan.run_dir / "default.dronebank.json"
    if args.dry_run:
        print(f"Would copy {source_bank} -> {plan.bank_asset}")
        return

    if not source_bank.exists():
        raise SystemExit(f"fit did not produce expected bank: {source_bank}")
    plan.bank_asset.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source_bank, plan.bank_asset)
    print(f"Copied fitted bank: {plan.bank_asset}")


def clean_derivatives(
    plan: SamplePlan,
    *,
    project_root: Path,
    immutable_root: Path,
    dry_run: bool,
) -> None:
    paths = [
        plan.bank_asset,
        plan.run_dir,
        project_root / "reports" / f"{plan.stem}_inspect",
        project_root / "reports" / f"{plan.stem}_init_bank",
        project_root / "reports" / f"{plan.stem}_init_render",
        project_root / "reports" / f"{plan.stem}_init_compare",
        project_root / "renders" / f"{plan.stem}_init.wav",
    ]
    for path in paths:
        remove_derivative(path, immutable_root=immutable_root, dry_run=dry_run)


def remove_derivative(path: Path, *, immutable_root: Path, dry_run: bool) -> None:
    resolved = path.resolve()
    if is_relative_to(resolved, immutable_root):
        raise SystemExit(f"refusing to remove path inside immutable audio root: {resolved}")
    if not path.exists():
        return
    if dry_run:
        print(f"Would remove {path}")
        return
    if path.is_dir():
        shutil.rmtree(path)
    else:
        path.unlink()
    print(f"Removed {path}")


def build_and_install(args: argparse.Namespace, project_root: Path) -> None:
    if args.skip_build:
        return

    build_dir = project_root / args.build_dir
    if not (build_dir / "CMakeCache.txt").exists():
        run(["cmake", "-S", str(project_root), "-B", str(build_dir)], dry_run=args.dry_run)

    run(["cmake", "--build", str(build_dir)], dry_run=args.dry_run)

    if args.skip_install:
        return

    if platform.system() != "Darwin":
        raise SystemExit("VST3 install/sign step is currently macOS-only; pass --skip-install.")

    install_dir = args.install_dir.expanduser()
    for build_product, bundle_name in VST3_BUILD_PRODUCTS:
        source = project_root / build_product
        destination = install_dir / bundle_name
        if args.dry_run:
            print(f"Would install {source} -> {destination}")
        else:
            if not source.exists():
                raise SystemExit(f"built VST3 bundle is missing: {source}")
            install_dir.mkdir(parents=True, exist_ok=True)
            run(["ditto", str(source), str(destination)], dry_run=False)
        if not args.skip_sign:
            run(["codesign", "--force", "--deep", "--sign", "-", str(destination)], dry_run=args.dry_run)
            run(["codesign", "--verify", "--deep", "--strict", str(destination)], dry_run=args.dry_run)


def run(command: Sequence[str], *, dry_run: bool) -> None:
    printable = " ".join(shell_quote(part) for part in command)
    if dry_run:
        print(f"Would run: {printable}")
        return
    print(f"Running: {printable}")
    subprocess.run(command, check=True)


def normalize_sample_selector(value: str) -> str:
    return Path(value).stem


def sample_sort_key(path: Path) -> tuple[str, int, str]:
    return sample_stem_sort_key(path.stem)


def sample_stem_sort_key(stem: str) -> tuple[str, int, str]:
    match = re.fullmatch(r"(.+?)(\d+)", stem)
    if match:
        return (match.group(1), int(match.group(2)), stem)
    return (stem, -1, stem)


def is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
    except ValueError:
        return False
    return True


def shell_quote(value: str) -> str:
    if re.fullmatch(r"[A-Za-z0-9_./:=,+-]+", value):
        return value
    return "'" + value.replace("'", "'\"'\"'") + "'"


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode) from error
