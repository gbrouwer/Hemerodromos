# Hemerodromos Drone

Hemerodromos Drone is a beta JUCE Standalone and VST3 instrument for fitting
drone source recordings into compact, real-time synthesizer banks.

The current beta plugin is branded `UMOJA DR-001`. It supports up to four
independent drone layers, profile-aware fitted soundbanks, neutral-centered
controls, preset save/load, and a rack-style GUI.

## Current Beta

Start here:

- [Beta release notes](docs/beta-release.md)
- [Full install-to-soundbank walkthrough](docs/walkthrough-install-to-soundbank.md)
- [Soundbank update pipeline](docs/soundbank-update-pipeline.md)
- [Fitting model notes](docs/dronefit-fitting.md)

## Local Setup

```bash
git submodule update --init --recursive
uv sync --dev
uv run python scripts/check_capabilities.py --strict
```

Build the plugin:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

The VST3 build output is:

```text
build/HemerodromosDrone_artefacts/RelWithDebInfo/VST3/Hemerodromos Drone.vst3
```

## Updating The Soundbank

User-provided audio belongs under `audio/base/`. Treat that directory as
protected source material. Generated outputs under `runs/`, `reports/`,
`renders/`, `assets/fitted/`, `build/`, and the installed VST3 bundle are
replaceable.

Check status:

```bash
uv run python scripts/update_soundbank.py --mode status
```

Process new samples and rebuild/install the VST3:

```bash
uv run python scripts/update_soundbank.py --mode new --yes
```

Regenerate both current comparison profiles:

```bash
uv run python scripts/update_soundbank.py --mode all \
  --fit-version v001_mrstft \
  --duplicate-policy warn \
  --yes

uv run python scripts/update_soundbank.py --mode all \
  --fit-version v001_srstft \
  --no-multi-resolution-stft-loss \
  --stft-fft-sizes 2048 \
  --duplicate-policy warn \
  --yes
```

## Documentation Site

The Hugo/Hextra docs source lives in `docs-site/` and is regenerated from
Markdown files under `docs/`.

Build locally:

```bash
scripts/build_docs_site.sh
```

Serve locally:

```bash
scripts/serve_docs_site.sh
```

## GitLab Pages

GitLab Pages is configured in [.gitlab-ci.yml](.gitlab-ci.yml). When the default
branch is pushed to GitLab, the `pages` job builds the Hextra docs site and
publishes the `public/` artifact.

Expected Pages URL for the existing GitLab project:

```text
https://umoja-group.gitlab.io/hemerodromos/
```

The local Hugo config contains a placeholder `baseURL`, but the GitLab pipeline
overrides it with `CI_PAGES_URL`, so the published site should use the correct
GitLab Pages URL automatically.

## Validation

```bash
uv run pytest
cmake --build build
scripts/build_docs_site.sh
```
