#!/usr/bin/env python3
"""Fit new drone samples and rebuild/reinstall the Hemerodromos VST3.

The source audio tree under audio/base is treated as immutable input. This
script only reads from it; cleanup is restricted to generated derivative paths.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
import platform
import re
import shutil
import subprocess
from collections.abc import Sequence


AUDIO_EXTENSIONS = {".wav", ".aif", ".aiff", ".flac"}
DEFAULT_SAMPLE_ROOT = Path("audio/base")
DEFAULT_FIT_VERSION = "v001"
FIT_VERSION_PATTERN = re.compile(r"[A-Za-z0-9][A-Za-z0-9_-]*")
DEFAULT_VST3_INSTALL_DIR = Path("~/Library/Audio/Plug-Ins/VST3").expanduser()
VST3_BUILD_PRODUCTS = (
    (
        Path("build/HemerodromosDrone_artefacts/RelWithDebInfo/VST3/Hemerodromos Drone.vst3"),
        "Hemerodromos Drone.vst3",
    ),
)


@dataclass(frozen=True)
class SamplePlan:
    sample: Path
    stem: str
    fit_version: str
    content_hash: str
    bank_asset: Path
    run_dir: Path
    processed: bool


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    validate_fit_version(args.fit_version)
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
    handle_duplicate_samples(all_plans, selected=selected, args=args)
    if args.mode == "status":
        return 0

    if not selected:
        print("No samples selected for processing.")
        if args.force_build:
            build_and_install(args, project_root)
        return 0

    if needs_confirmation(selected, args) and not confirm_reprocess(selected):
        raise SystemExit("Aborted.")

    approved_count = 0
    for plan in selected:
        if process_sample(plan, args, project_root, immutable_root):
            approved_count += 1

    if not args.skip_build:
        if approved_count == 0 and not args.force_build and not args.dry_run:
            print("No fitted banks were approved for embedding; skipping build/install.")
        else:
            build_and_install(args, project_root)

    return 0


def parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Update fitted drone banks from audio/base samples, then rebuild and reinstall "
            "the Hemerodromos VST3 plugin."
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
    parser.add_argument("--max-frequency-offset-cents", type=float, default=20.0)
    parser.add_argument("--max-frequency-motion-cents", type=float, default=12.0)
    parser.add_argument(
        "--residual-noise-bands",
        type=int,
        default=0,
        help="Number of residual filtered-noise bands to export when --residual-noise is enabled.",
    )
    parser.add_argument(
        "--sub-bin-peaks",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Use sub-bin peak interpolation during initialization.",
    )
    parser.add_argument(
        "--adaptive-partial-count",
        dest="adaptive_partial_count",
        action="store_true",
        default=False,
        help="Allow the detected partial count to fall below --partials when weak peaks are rejected.",
    )
    parser.add_argument(
        "--fixed-partial-count",
        "--no-adaptive-partial-count",
        dest="adaptive_partial_count",
        action="store_false",
        help="Use the legacy fixed FFT-bin peak detector without weak-peak pruning.",
    )
    parser.add_argument(
        "--frequency-offsets",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Fit static per-partial frequency offsets.",
    )
    parser.add_argument(
        "--frequency-motion",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Fit slow per-partial frequency motion.",
    )
    parser.add_argument(
        "--multi-resolution-stft-loss",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use all configured STFT FFT sizes for spectral loss.",
    )
    parser.add_argument(
        "--stereo-fit",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Estimate stereo width from the source sample.",
    )
    parser.add_argument(
        "--residual-noise",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Export residual filtered-noise bands. Disabled by default.",
    )
    parser.add_argument("--device", choices=("auto", "cpu", "mps"), default="auto")
    parser.add_argument("--report-every", type=int, default=100)
    parser.add_argument("--report-seconds", type=float, default=10.0)
    parser.add_argument("--stft-fft-sizes", default="512,2048,8192")
    parser.add_argument("--duplicate-policy", choices=("fail", "warn", "ignore"), default="fail")
    parser.add_argument("--quality-gate", choices=("off", "warn", "fail"), default="warn")
    parser.add_argument("--max-residual-rms", type=float, default=0.34)
    parser.add_argument("--max-mean-stft-delta", type=float, default=8.0)
    parser.add_argument("--min-partials", type=int, default=24)
    parser.add_argument(
        "--require-approval",
        action="store_true",
        help="Prompt before copying a newly fitted bank into assets/fitted.",
    )
    parser.add_argument(
        "--approve",
        action="store_true",
        help="Auto-approve quality-passing banks when --require-approval is set.",
    )
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
                fit_version=fit_version,
                content_hash=sha256_file(sample),
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


def handle_duplicate_samples(
    all_plans: Sequence[SamplePlan],
    *,
    selected: Sequence[SamplePlan],
    args: argparse.Namespace,
) -> None:
    groups = duplicate_hash_groups(all_plans)
    if not groups or args.duplicate_policy == "ignore":
        return

    selected_hashes = {plan.content_hash for plan in selected}
    relevant_groups = [
        group
        for group in groups
        if args.mode == "status" or any(plan.content_hash in selected_hashes for plan in group)
    ]
    if not relevant_groups:
        return

    lines = [
        "Duplicate source sample content detected:",
        *(
            "  " + ", ".join(plan.stem for plan in group)
            for group in relevant_groups
        ),
    ]
    message = "\n".join(lines)
    if args.mode != "status" and args.duplicate_policy == "fail":
        raise SystemExit(message + "\nUse --duplicate-policy warn or remove the duplicate source file.")
    print(message)


def duplicate_hash_groups(plans: Sequence[SamplePlan]) -> list[list[SamplePlan]]:
    by_hash: dict[str, list[SamplePlan]] = {}
    for plan in plans:
        by_hash.setdefault(plan.content_hash, []).append(plan)
    return [group for group in by_hash.values() if len(group) > 1]


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
) -> bool:
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
        "--max-frequency-offset-cents",
        str(args.max_frequency_offset_cents),
        "--max-frequency-motion-cents",
        str(args.max_frequency_motion_cents),
        "--residual-noise-bands",
        str(args.residual_noise_bands),
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
    append_bool_flag(fit_command, args.sub_bin_peaks, "--sub-bin-peaks", "--no-sub-bin-peaks")
    append_bool_flag(
        fit_command,
        args.adaptive_partial_count,
        "--adaptive-partial-count",
        "--fixed-partial-count",
    )
    append_bool_flag(fit_command, args.frequency_offsets, "--frequency-offsets", "--no-frequency-offsets")
    append_bool_flag(fit_command, args.frequency_motion, "--frequency-motion", "--no-frequency-motion")
    append_bool_flag(
        fit_command,
        args.multi_resolution_stft_loss,
        "--multi-resolution-stft-loss",
        "--single-resolution-stft-loss",
    )
    append_bool_flag(fit_command, args.stereo_fit, "--stereo-fit", "--no-stereo-fit")
    append_bool_flag(fit_command, args.residual_noise, "--residual-noise", "--no-residual-noise")
    run(fit_command, dry_run=args.dry_run)

    source_bank = plan.run_dir / "default.dronebank.json"
    if args.dry_run:
        print(f"Would copy {source_bank} -> {plan.bank_asset}")
        return False

    if not source_bank.exists():
        raise SystemExit(f"fit did not produce expected bank: {source_bank}")

    quality = read_quality_summary(plan)
    if not quality_is_approved(plan, quality=quality, args=args):
        print(f"Not embedding unapproved fitted bank for {plan.stem}; run artifacts remain in {plan.run_dir}")
        return False

    plan.bank_asset.parent.mkdir(parents=True, exist_ok=True)
    write_approved_bank_asset(source_bank, plan=plan, quality=quality)
    print(f"Copied fitted bank: {plan.bank_asset}")
    return True


def clean_derivatives(
    plan: SamplePlan,
    *,
    project_root: Path,
    immutable_root: Path,
    dry_run: bool,
) -> None:
    paths = [
        plan.run_dir,
        project_root / "reports" / f"{plan.stem}_inspect",
        project_root / "reports" / f"{plan.stem}_init_bank",
        project_root / "reports" / f"{plan.stem}_init_render",
        project_root / "reports" / f"{plan.stem}_init_compare",
        project_root / "renders" / f"{plan.stem}_init.wav",
    ]
    for path in paths:
        remove_derivative(path, immutable_root=immutable_root, dry_run=dry_run)


def read_quality_summary(plan: SamplePlan) -> dict[str, float | int | str | None]:
    metrics_path = plan.run_dir / "reports" / "final" / "comparison_metrics.json"
    bank_metrics_path = plan.run_dir / "reports" / "final" / "bank_metrics.json"
    if not metrics_path.exists():
        return {
            "residual_rms": None,
            "mean_abs_stft_db_delta": None,
            "median_abs_stft_db_delta": None,
            "partials": None,
            "status": "missing_metrics",
        }

    metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
    bank_metrics = (
        json.loads(bank_metrics_path.read_text(encoding="utf-8")) if bank_metrics_path.exists() else {}
    )
    bank_info = metrics.get("bank", {}) if isinstance(metrics.get("bank"), dict) else {}
    return {
        "residual_rms": metrics.get("residual_rms"),
        "mean_abs_stft_db_delta": metrics.get("mean_abs_stft_db_delta"),
        "median_abs_stft_db_delta": metrics.get("median_abs_stft_db_delta"),
        "partials": bank_info.get("partials", bank_metrics.get("partials")),
        "status": "ok",
    }


def quality_is_approved(
    plan: SamplePlan,
    *,
    quality: dict[str, float | int | str | None],
    args: argparse.Namespace,
) -> bool:
    issues = quality_issues(quality, args=args)
    summary = (
        f"Quality {plan.stem}: partials={quality.get('partials')} "
        f"residual_rms={format_metric(quality.get('residual_rms'))} "
        f"mean_stft_delta={format_metric(quality.get('mean_abs_stft_db_delta'))}dB "
        f"median_stft_delta={format_metric(quality.get('median_abs_stft_db_delta'))}dB"
    )
    print(summary)
    if issues:
        print("Quality issue(s): " + "; ".join(issues))
        if args.quality_gate == "fail":
            return False

    if not args.require_approval:
        return True
    if args.approve and not issues:
        print(f"Auto-approved {plan.stem}.")
        return True

    response = input(f"Approve {plan.stem} for embedding in assets/fitted? [y/N] ").strip().lower()
    return response in {"y", "yes"}


def quality_issues(
    quality: dict[str, float | int | str | None],
    *,
    args: argparse.Namespace,
) -> list[str]:
    if args.quality_gate == "off":
        return []

    issues: list[str] = []
    residual = quality.get("residual_rms")
    mean_delta = quality.get("mean_abs_stft_db_delta")
    partials = quality.get("partials")
    if residual is None or mean_delta is None or partials is None:
        issues.append("missing final comparison metrics")
        return issues

    if float(residual) > args.max_residual_rms:
        issues.append(f"residual RMS {float(residual):.4f} > {args.max_residual_rms:.4f}")
    if float(mean_delta) > args.max_mean_stft_delta:
        issues.append(f"mean STFT delta {float(mean_delta):.2f}dB > {args.max_mean_stft_delta:.2f}dB")
    if int(partials) < args.min_partials:
        issues.append(f"partials {int(partials)} < {args.min_partials}")
    return issues


def write_approved_bank_asset(
    source_bank: Path,
    *,
    plan: SamplePlan,
    quality: dict[str, float | int | str | None],
) -> None:
    data = json.loads(source_bank.read_text(encoding="utf-8"))
    profile_label = profile_label_from_fit_version(plan.fit_version)
    display_name = display_name_from_stem(plan.stem, profile_label=profile_label)
    data["name"] = display_name
    metadata = data.setdefault("metadata", {})
    metadata["soundbank"] = {
        "approved_at": datetime.now(timezone.utc).replace(microsecond=0).isoformat(),
        "display_name": display_name,
        "fit_version": plan.fit_version,
        "fit_profile_label": profile_label,
        "source_sha256": plan.content_hash,
        "source_stem": plan.stem,
    }
    metadata["quality"] = quality
    plan.bank_asset.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def display_name_from_stem(stem: str, *, profile_label: str = "") -> str:
    match = re.fullmatch(r"drone_base0*(\d+)", stem)
    if match:
        base_name = f"Drone {int(match.group(1))}"
    else:
        base_name = " ".join(part for part in re.split(r"[_\-\s]+", stem.strip()) if part).title() or "Drone"

    return f"{base_name} {profile_label}".strip()


def validate_fit_version(fit_version: str) -> None:
    if not FIT_VERSION_PATTERN.fullmatch(fit_version):
        raise SystemExit(
            "--fit-version must contain only letters, digits, underscores, and hyphens, "
            "and must start with a letter or digit."
        )


def profile_label_from_fit_version(fit_version: str) -> str:
    if fit_version == DEFAULT_FIT_VERSION:
        return ""

    match = re.fullmatch(r"v\d+_(.+)", fit_version, flags=re.IGNORECASE)
    profile = match.group(1) if match else fit_version
    normalized = profile.lower().replace("-", "_")
    known = {
        "mrstft": "MR-STFT",
        "multi_stft": "MR-STFT",
        "multi_resolution_stft": "MR-STFT",
        "srstft": "SR-STFT",
        "single_stft": "SR-STFT",
        "single_resolution_stft": "SR-STFT",
    }
    if normalized in known:
        return known[normalized]

    words = [word for word in re.split(r"[_\-\s]+", profile.strip()) if word]
    if not words:
        return fit_version.upper()
    return " ".join(word.upper() if len(word) <= 3 else word.title() for word in words)


def format_metric(value: float | int | str | None) -> str:
    if isinstance(value, int | float):
        return f"{float(value):.4f}"
    return "-"


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


def append_bool_flag(command: list[str], enabled: bool, enabled_flag: str, disabled_flag: str) -> None:
    command.append(enabled_flag if enabled else disabled_flag)


def normalize_sample_selector(value: str) -> str:
    return Path(value).stem


def sample_sort_key(path: Path) -> tuple[str, int, str]:
    return sample_stem_sort_key(path.stem)


def sample_stem_sort_key(stem: str) -> tuple[str, int, str]:
    match = re.fullmatch(r"(.+?)(\d+)", stem)
    if match:
        return (match.group(1), int(match.group(2)), stem)
    return (stem, -1, stem)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


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
