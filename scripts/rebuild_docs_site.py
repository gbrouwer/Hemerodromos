#!/usr/bin/env python3
"""Regenerate Hugo/Hextra docs content from repository Markdown docs."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import shutil


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DOCS_ROOT = PROJECT_ROOT / "docs"
SITE_ROOT = PROJECT_ROOT / "docs-site"
CONTENT_ROOT = SITE_ROOT / "content"
STATIC_ROOT = SITE_ROOT / "static"


@dataclass(frozen=True)
class DocPage:
    source: str
    target: str
    title: str
    weight: int


DOC_PAGES = [
    DocPage(
        "beta-release.md",
        "beta-release.md",
        "Beta Release",
        5,
    ),
    DocPage(
        "walkthrough-install-to-soundbank.md",
        "walkthrough.md",
        "Full Walkthrough",
        10,
    ),
    DocPage(
        "setup-and-capability-checks.md",
        "setup-and-capability-checks.md",
        "Setup And Capability Checks",
        20,
    ),
    DocPage(
        "soundbank-update-pipeline.md",
        "soundbank-update-pipeline.md",
        "Soundbank Update Pipeline",
        30,
    ),
    DocPage(
        "dronefit-fitting.md",
        "dronefit-fitting.md",
        "Dronefit Fitting",
        40,
    ),
    DocPage(
        "dronefit-reports.md",
        "dronefit-reports.md",
        "Dronefit Reports",
        50,
    ),
    DocPage(
        "instructions/CODEX_PROJECT_HANDOFF_drone_vst3_with_gui.md",
        "project-handoff.md",
        "Project Handoff",
        90,
    ),
]


def main() -> int:
    docs_content = CONTENT_ROOT / "docs"
    if docs_content.exists():
        shutil.rmtree(docs_content)
    docs_content.mkdir(parents=True, exist_ok=True)

    write_homepage()
    write_docs_index(docs_content)
    for page in DOC_PAGES:
        write_page(page, docs_content / page.target)

    copy_static_assets()
    print(f"Regenerated Hugo docs content in {SITE_ROOT}")
    return 0


def write_homepage() -> None:
    CONTENT_ROOT.mkdir(parents=True, exist_ok=True)
    (CONTENT_ROOT / "_index.md").write_text(
        """---
title: Hemerodromos
type: hextra-home
---

Offline drone fitting tools plus JUCE Standalone and VST3 instruments.

<div style="text-align: center; margin: 1.5rem auto 2rem;">
  <img src="/assets/DR1-001.svg" alt="UMOJA DR-001 logo" style="width: min(420px, 78vw); margin: 0 auto 1.25rem;" />
  <img src="/assets/vst3-screenshot.png" alt="Hemerodromos Drone VST3 interface" style="width: min(1040px, 100%); margin: 0 auto; border-radius: 10px; box-shadow: 0 18px 50px rgba(0, 0, 0, 0.22);" />
</div>

{{< cards >}}
  {{< card link=\"docs/beta-release/\" title=\"Beta Release\" subtitle=\"Current VST3 instrument state, soundbank, presets, and beta expectations.\" >}}
  {{< card link=\"docs/walkthrough/\" title=\"Full Walkthrough\" subtitle=\"Install, add sounds, fit banks, rebuild plugins, and publish docs.\" >}}
  {{< card link=\"docs/soundbank-update-pipeline/\" title=\"Soundbank Updates\" subtitle=\"Run the repeatable pipeline for new or selected samples.\" >}}
  {{< card link=\"docs/dronefit-fitting/\" title=\"Dronefit\" subtitle=\"Understand the fitting model and generated bank format.\" >}}
{{< /cards >}}
""",
        encoding="utf-8",
    )


def write_docs_index(docs_content: Path) -> None:
    (docs_content / "_index.md").write_text(
        """---
title: Documentation
weight: 1
---

# Documentation

Start with the beta release note and full walkthrough, then use the focused
pages for fitting, reports, and soundbank maintenance details.
""",
        encoding="utf-8",
    )


def write_page(page: DocPage, destination: Path) -> None:
    source = DOCS_ROOT / page.source
    if not source.exists():
        raise SystemExit(f"missing source doc: {source}")

    text = source.read_text(encoding="utf-8")
    text = strip_initial_h1(text)
    text = rewrite_asset_paths(text)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(
        f"""---
title: {page.title}
weight: {page.weight}
---

{text}
""",
        encoding="utf-8",
    )


def rewrite_asset_paths(text: str) -> str:
    return text.replace("walkthrough-assets/", "/walkthrough-assets/")


def strip_initial_h1(text: str) -> str:
    lines = text.splitlines()
    if lines and lines[0].startswith("# "):
        lines = lines[1:]
        if lines and not lines[0].strip():
            lines = lines[1:]
    return "\n".join(lines).rstrip() + "\n"


def copy_static_assets() -> None:
    for source_name, destination_name in (
        ("walkthrough-assets", "walkthrough-assets"),
        ("assets", "assets"),
    ):
        source = DOCS_ROOT / source_name
        destination = STATIC_ROOT / destination_name
        if destination.exists():
            shutil.rmtree(destination)
        if source.exists():
            shutil.copytree(source, destination)


if __name__ == "__main__":
    raise SystemExit(main())
