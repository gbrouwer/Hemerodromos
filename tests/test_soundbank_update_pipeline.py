from __future__ import annotations

import importlib.util
from pathlib import Path
import sys

import pytest

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "scripts" / "update_soundbank.py"
SPEC = importlib.util.spec_from_file_location("update_soundbank", SCRIPT_PATH)
assert SPEC is not None
update_soundbank = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = update_soundbank
SPEC.loader.exec_module(update_soundbank)

discover_sample_plans = update_soundbank.discover_sample_plans
display_name_from_stem = update_soundbank.display_name_from_stem
duplicate_hash_groups = update_soundbank.duplicate_hash_groups
profile_label_from_fit_version = update_soundbank.profile_label_from_fit_version
remove_derivative = update_soundbank.remove_derivative
select_plans = update_soundbank.select_plans


def test_select_new_samples_uses_missing_fitted_assets(tmp_path: Path):
    sample_root = tmp_path / "audio" / "base" / "drones"
    sample_root.mkdir(parents=True)
    (sample_root / "drone_base1.wav").write_bytes(b"sample")
    (sample_root / "drone_base2.wav").write_bytes(b"sample")
    fitted_dir = tmp_path / "assets" / "fitted"
    fitted_dir.mkdir(parents=True)
    (fitted_dir / "drone_base1_fit_v001.dronebank.json").write_text("{}")

    plans = discover_sample_plans(
        project_root=tmp_path,
        sample_root=sample_root,
        fit_version="v001",
    )
    selected = select_plans(plans, mode="new", samples=())

    assert [plan.stem for plan in plans] == ["drone_base1", "drone_base2"]
    assert [plan.stem for plan in selected] == ["drone_base2"]


def test_select_specific_samples_sorts_by_numeric_suffix(tmp_path: Path):
    sample_root = tmp_path / "audio" / "base" / "drones"
    sample_root.mkdir(parents=True)
    for name in ["drone_base10.wav", "drone_base2.wav", "drone_base1.wav"]:
        (sample_root / name).write_bytes(b"sample")

    plans = discover_sample_plans(
        project_root=tmp_path,
        sample_root=sample_root,
        fit_version="v001",
    )
    selected = select_plans(
        plans,
        mode="selected",
        samples=("drone_base10.wav", "drone_base1"),
    )

    assert [plan.stem for plan in plans] == ["drone_base1", "drone_base2", "drone_base10"]
    assert [plan.stem for plan in selected] == ["drone_base1", "drone_base10"]


def test_remove_derivative_refuses_immutable_audio_root(tmp_path: Path):
    immutable_root = tmp_path / "audio" / "base"
    immutable_root.mkdir(parents=True)
    sample = immutable_root / "drones" / "drone_base1.wav"
    sample.parent.mkdir()
    sample.write_bytes(b"sample")

    with pytest.raises(SystemExit):
        remove_derivative(sample, immutable_root=immutable_root.resolve(), dry_run=False)

    assert sample.exists()


def test_duplicate_hash_groups_detect_same_audio_content(tmp_path: Path):
    sample_root = tmp_path / "audio" / "base" / "drones"
    sample_root.mkdir(parents=True)
    (sample_root / "drone_base1.wav").write_bytes(b"same")
    (sample_root / "drone_base2.wav").write_bytes(b"same")
    (sample_root / "drone_base3.wav").write_bytes(b"different")

    plans = discover_sample_plans(
        project_root=tmp_path,
        sample_root=sample_root,
        fit_version="v001",
    )
    groups = duplicate_hash_groups(plans)

    assert [[plan.stem for plan in group] for group in groups] == [["drone_base1", "drone_base2"]]


def test_display_name_from_stem_cleans_drone_base_names():
    assert display_name_from_stem("drone_base13") == "Drone 13"
    assert display_name_from_stem("drone_base13", profile_label="MR-STFT") == "Drone 13 MR-STFT"
    assert display_name_from_stem("my_dark_drone") == "My Dark Drone"


def test_fit_version_profile_label_is_human_readable():
    assert profile_label_from_fit_version("v001") == ""
    assert profile_label_from_fit_version("v001_mrstft") == "MR-STFT"
    assert profile_label_from_fit_version("v001_srstft") == "SR-STFT"
    assert profile_label_from_fit_version("v002_freq_motion") == "Freq Motion"


def test_fit_version_changes_run_and_asset_paths_without_touching_source(tmp_path: Path):
    sample_root = tmp_path / "audio" / "base" / "drones"
    sample_root.mkdir(parents=True)
    sample = sample_root / "drone_base15.wav"
    sample.write_bytes(b"sample")

    plans = discover_sample_plans(
        project_root=tmp_path,
        sample_root=sample_root,
        fit_version="v001_srstft",
    )

    assert plans[0].sample == sample
    assert plans[0].fit_version == "v001_srstft"
    assert plans[0].bank_asset == (
        tmp_path / "assets" / "fitted" / "drone_base15_fit_v001_srstft.dronebank.json"
    )
    assert plans[0].run_dir == tmp_path / "runs" / "drone_base15_fit_v001_srstft"
