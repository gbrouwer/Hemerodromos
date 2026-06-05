---
title: Project Handoff
weight: 90
---

**Working project name:** `drone-vst3`  
**Document purpose:** Give a CODEX / ChatGPT coding assistant enough context, plan, tooling, constraints, and implementation steps to pick up the project inside a fresh working folder and execute it.  
**Last updated:** 2026-06-02  
**Primary deliverable:** A VST3 instrument plugin for Ableton Live that synthesizes a drone-like sound learned from a user-provided audio sample, with musically meaningful parameters exposed to the DAW and to a polished knob-based GUI for static or automated modulation.

---

## 1. Project summary

The user has recordings containing a predominantly drone-like sound. The sound modulates around a stable timbral identity but may not have a clear conventional musical evolution. The goal is **not** to make a simple looping sample player. The goal is to extract an expressive, lower-dimensional synthesis model from a reasonably long sample and build a VST3 instrument that can recreate the character of the source while exposing controllable parameters through both Ableton automation and a tactile, synth-like GUI.

The core idea is:

1. The user provides a target audio sample, preferably WAV/AIFF, mono or stereo, roughly 5–60 seconds to start.
2. Offline Python tooling analyzes and fits the target using a compact model. Start with interpretable additive / spectral / basis-function fitting. Use PyTorch and GPU acceleration where available.
3. The fitting stage exports a compact, versioned asset: coefficients, frequencies, amplitudes, modulation envelopes, noise-band data, and macro-control mappings.
4. A real-time C++ VST3 instrument, built with JUCE and CMake, loads this asset and renders it safely in Ableton Live.
5. Plugin parameters allow the user to change the sound statically or dynamically over time: brightness, motion depth, motion rate, spectral shift, roughness/noise, detune, stereo width, gain, note transposition, etc. These parameters must be surfaced as real DAW parameters and as rotary controls in a custom GUI.

Recommended architecture: **offline ML/DSP in Python; real-time synthesis in C++ without Python/PyTorch inside the audio callback**. PyTorch is excellent for fitting, but embedding it directly in a VST3 is heavy, fragile, and unnecessary for the MVP.

---

## 2. Important assumptions and constraints

- Target DAW: **Ableton Live**.
- Target plugin format: **VST3 instrument**. AU can be added later on macOS, but do not let AU distract from the first VST3 target.
- Target OS: unknown. Make the repo cross-platform where feasible. Prioritize macOS and Windows because Ableton users commonly need those. Linux can be useful for development but Ableton Live is not the main target there.
- Build system: **CMake**.
- C++ framework: **JUCE** is recommended for the plugin layer. Direct Steinberg VST3 SDK development is possible but slower for this project.
- GUI: **required**, not optional polish. The first useful plugin should have a custom JUCE editor with rotary knobs, labels, visual grouping, and host-synced automation.
- Visual direction: inspired by premium synth plugin interfaces such as Arturia instruments and Analog Lab, but do **not** copy their protected artwork, layout, branding, trade dress, fonts, icons, or presets. Build an original compact interface with similar clarity and tactile affordances.
- Python environment: **uv**.
- Offline fitting: **PyTorch**, ideally GPU-enabled if the host machine supports CUDA or ROCm. CPU mode must still work for small tests.
- Audio thread rule: no allocations, no locks, no file I/O, no Python, no GPU work, no slow model inference inside `processBlock`.
- Audio assets and recordings may be large or private. Do not commit raw recordings to git. Use `.gitignore`, Git LFS, DVC, or a local `data/raw/` folder.
- Licensing matters. Verify JUCE and VST3 licensing before binary distribution. For a private/local prototype this is simpler, but still document choices.

---

## 3. Current external facts to verify before implementation

CODEX should re-check these URLs if the project is being executed at a later date, because tool installation details can change.

- uv project docs: <https://docs.astral.sh/uv/guides/projects/>
- PyTorch local install selector: <https://pytorch.org/get-started/locally/>
- JUCE CMake API: <https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md>
- JUCE AudioProcessorEditor docs: <https://docs.juce.com/master/classjuce_1_1AudioProcessorEditor.html>
- JUCE Slider docs, including rotary sliders: <https://docs.juce.com/master/classjuce_1_1Slider.html>
- JUCE AudioProcessorValueTreeState tutorial: <https://juce.com/tutorials/tutorial_audio_processor_value_tree_state/>
- JUCE SliderAttachment docs: <https://docs.juce.com/master/classjuce_1_1AudioProcessorValueTreeState_1_1SliderAttachment.html>
- JUCE LookAndFeel customisation tutorial: <https://juce.com/tutorials/tutorial_look_and_feel_customisation/>
- JUCE responsive layout tutorial, FlexBox and Grid: <https://juce.com/tutorials/tutorial_flex_box_grid/>
- JUCE licensing: <https://github.com/juce-framework/JUCE/blob/master/LICENSE.md>
- Steinberg VST3 SDK: <https://github.com/steinbergmedia/vst3sdk>
- Ableton Live VST plugins on Windows: <https://help.ableton.com/hc/en-us/articles/209071729-Using-VST-plug-ins-on-Windows>
- Ableton Live AU/VST plugins on macOS: <https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS>
- pluginval: <https://github.com/Tracktion/pluginval>
- ONNX Runtime, optional later-stage deployment: <https://github.com/microsoft/onnxruntime>
- DDSP reference paper: <https://arxiv.org/abs/2001.04643>
- librosa feature docs: <https://librosa.org/doc/stable/feature.html>

---

## 4. First-session priorities for CODEX

Do these in order. Make small commits after each working milestone.

### 4.1 Create repo bookkeeping

```bash
mkdir drone-vst3
cd drone-vst3
git init
```

Create these files immediately:

```text
README.md
CODEX_PROJECT_HANDOFF.md       # copy this document here
PROJECT_LOG.md                 # append dated progress notes here
TODO.md                        # short checklist of next actions
docs/decision-log.md           # record key architecture decisions
docs/references.md             # external links and versions checked
.gitignore
```

Suggested `.gitignore`:

```gitignore
# Python
.venv/
__pycache__/
*.py[cod]
.pytest_cache/
.ruff_cache/
.mypy_cache/
.ipynb_checkpoints/

# Fitting outputs and private audio
/data/raw/
/data/private/
/runs/
/renders/
/reports/
*.wav
*.aif
*.aiff
*.flac
*.mp3

# C++ build outputs
/build*/
/cmake-build-*/
*.vst3
*.component
*.dll
*.dylib
*.so
*.pdb
*.ilk
*.obj
*.o

# OS/editor
.DS_Store
.vscode/
.idea/
```

Commit:

```bash
git add .
git commit -m "Initialize project bookkeeping"
```

### 4.2 Set up Python with uv

Use uv for Python project management and lockfiles.

```bash
uv init
uv python pin 3.12
uv sync
```

Add baseline Python dependencies:

```bash
uv add numpy scipy soundfile librosa matplotlib tqdm rich typer pydantic
uv add --dev pytest ruff mypy ipykernel
```

Install PyTorch using the official PyTorch install selector for the machine. Do not guess the CUDA/ROCm wheel. A safe CPU fallback is acceptable for initial tests, but GPU should be used for real fitting runs if available.

Examples, to be adapted after checking the official selector:

```bash
# CPU-ish/simple path, suitable for a smoke test:
uv add torch torchaudio

# GPU path: get the exact current command from https://pytorch.org/get-started/locally/
# Example shape only; do not blindly copy the CUDA version:
# uv pip install torch torchaudio --index-url https://download.pytorch.org/whl/cuXXX
```

Verify:

```bash
uv run python - <<'PY'
import torch
print("torch", torch.__version__)
print("cuda available", torch.cuda.is_available())
print("cuda version", getattr(torch.version, "cuda", None))
if torch.cuda.is_available():
    print("device", torch.cuda.get_device_name(0))
PY
```

Commit after the environment files are stable:

```bash
git add pyproject.toml uv.lock .python-version README.md TODO.md docs PROJECT_LOG.md
git commit -m "Set up Python project with uv"
```

### 4.3 Set up C++ / JUCE / CMake skeleton

Add JUCE as a submodule unless the user already has a preferred JUCE checkout:

```bash
git submodule add https://github.com/juce-framework/JUCE external/JUCE
git commit -m "Add JUCE submodule"
```

Install C++ tooling outside the repo:

- macOS: Xcode Command Line Tools, CMake, Ninja optional.
- Windows: Visual Studio 2022 Build Tools with C++ workload, CMake, Ninja optional.
- Linux/dev only: GCC or Clang, CMake, Ninja, ALSA/JACK dependencies if building standalone tests.

Create top-level `CMakeLists.txt` and plugin source folders:

```text
CMakeLists.txt
plugin/Source/PluginProcessor.h
plugin/Source/PluginProcessor.cpp
plugin/Source/PluginEditor.h
plugin/Source/PluginEditor.cpp
plugin/Source/SynthEngine.h
plugin/Source/SynthEngine.cpp
plugin/Source/DroneBank.h
plugin/Source/DroneBank.cpp
plugin/Source/ParameterIds.h
plugin/Source/UI/Theme.h
plugin/Source/UI/Theme.cpp
plugin/Source/UI/DroneLookAndFeel.h
plugin/Source/UI/DroneLookAndFeel.cpp
plugin/Source/UI/Knob.h
plugin/Source/UI/Knob.cpp
plugin/Source/UI/SectionPanel.h
plugin/Source/UI/SectionPanel.cpp
plugin/Source/UI/PartialDisplay.h
plugin/Source/UI/PartialDisplay.cpp
plugin/assets/default.dronebank.json
plugin/assets/images/.gitkeep
plugin/assets/fonts/.gitkeep        # only if using legally licensed redistributable fonts; otherwise use system fonts
```

Minimal CMake direction:

```cmake
cmake_minimum_required(VERSION 3.24)
project(DroneVST3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/JUCE)

juce_add_plugin(DroneVST3
    COMPANY_NAME "LocalPrototype"
    PRODUCT_NAME "DroneVST3"
    BUNDLE_ID "com.localprototype.dronevst3"
    VERSION "0.1.0"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3 Standalone
    VST3_CATEGORIES "Instrument" "Synth")

juce_generate_juce_header(DroneVST3)

target_sources(DroneVST3 PRIVATE
    plugin/Source/PluginProcessor.cpp
    plugin/Source/PluginEditor.cpp
    plugin/Source/SynthEngine.cpp
    plugin/Source/DroneBank.cpp
    plugin/Source/UI/Theme.cpp
    plugin/Source/UI/DroneLookAndFeel.cpp
    plugin/Source/UI/Knob.cpp
    plugin/Source/UI/SectionPanel.cpp
    plugin/Source/UI/PartialDisplay.cpp)

target_include_directories(DroneVST3 PRIVATE plugin/Source)

target_link_libraries(DroneVST3 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra)

target_compile_definitions(DroneVST3 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
```

If the GUI uses image assets, add them through JUCE/CMake binary data rather than loading files from arbitrary disk paths at runtime. Example shape:

```cmake
juce_add_binary_data(DroneVST3Assets
    SOURCES
        plugin/assets/images/logo.svg
        plugin/assets/images/background.png)

target_link_libraries(DroneVST3 PRIVATE DroneVST3Assets)
```

Build smoke tests:

```bash
# Ninja-style build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

# Windows Visual Studio alternative
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config RelWithDebInfo
```

Commit once the empty plugin builds:

```bash
git add CMakeLists.txt plugin external/.gitmodules .gitmodules
# Adjust files according to actual paths.
git commit -m "Add JUCE VST3 plugin skeleton"
```

---

## 5. Recommended repository structure

Use this as the target shape. Do not overfit to it if JUCE/CMake requires slightly different paths.

```text
drone-vst3/
├── README.md
├── CODEX_PROJECT_HANDOFF.md
├── PROJECT_LOG.md
├── TODO.md
├── pyproject.toml
├── uv.lock
├── .python-version
├── .gitignore
├── CMakeLists.txt
├── cmake/
│   └── Presets.cmake or toolchain helpers later
├── docs/
│   ├── decision-log.md
│   ├── references.md
│   ├── fitting-methods.md
│   ├── plugin-design.md
│   └── gui-design.md
├── design/
│   ├── mockups/               # optional Figma exports, PNGs, SVG sketches
│   └── moodboard.md           # references and original visual direction notes
├── data/
│   ├── raw/                  # gitignored user recordings
│   └── examples/             # tiny synthetic fixtures only
├── python/
│   └── dronefit/
│       ├── __init__.py
│       ├── cli.py
│       ├── audio_io.py
│       ├── inspect.py
│       ├── features.py
│       ├── synth.py
│       ├── losses.py
│       ├── fit.py
│       ├── export.py
│       └── schema.py
├── scripts/
│   ├── fit_sample.py         # optional wrappers
│   ├── render_bank.py
│   └── compare_audio.py
├── tests/
│   ├── test_dronebank_schema.py
│   ├── test_synth_shapes.py
│   └── test_export_roundtrip.py
├── plugin/
│   ├── Source/
│   │   ├── PluginProcessor.h/.cpp
│   │   ├── PluginEditor.h/.cpp
│   │   ├── SynthEngine.h/.cpp
│   │   ├── DroneBank.h/.cpp
│   │   ├── ParameterIds.h
│   │   ├── RealtimeSafe.h
│   │   └── UI/
│   │       ├── Theme.h/.cpp
│   │       ├── DroneLookAndFeel.h/.cpp
│   │       ├── Knob.h/.cpp
│   │       ├── SectionPanel.h/.cpp
│   │       └── PartialDisplay.h/.cpp
│   └── assets/
│       ├── default.dronebank.json
│       ├── images/
│       └── fonts/              # optional; only licensed redistributable fonts
├── assets/
│   └── fitted/               # exported model assets; small files can be tracked
├── runs/                     # gitignored fitting runs
├── renders/                  # gitignored rendered audio comparisons
├── reports/                  # gitignored spectrograms / metrics
└── external/
    └── JUCE/
```

---

## 6. MVP product definition

### 6.1 MVP user story

The user can:

1. Place a drone sample in `data/raw/target.wav`.
2. Run a fitting command.
3. Audition a rendered synthesized approximation as a WAV.
4. Export a compact `.dronebank.json` or `.dronebank.bin` asset.
5. Build/install a VST3 instrument.
6. Load the VST3 in Ableton Live.
7. Play MIDI notes that transpose the learned drone model.
8. Automate parameters in Ableton to vary the sound over time.

### 6.2 MVP constraints

- Monophonic synth is acceptable for the first version.
- Stereo is optional in the fitting stage. If input is stereo, start by mono-summing for fitting and synthesize stereo with a width/spread algorithm later.
- Use 48 kHz internally for initial analysis unless the source sample rate suggests otherwise. The plugin must render correctly at host sample rates 44.1, 48, 88.2, and 96 kHz.
- Do not require a neural net runtime in the plugin.
- The first fit may sound approximate; controllability and reliability are more important than perfect mimicry.

### 6.3 MVP acceptance criteria

A first successful version satisfies all of this:

- `uv run dronefit inspect data/raw/target.wav --out reports/target_inspect` creates spectrogram and feature plots.
- `uv run dronefit fit data/raw/target.wav --out assets/fitted/target_v001` creates a versioned model asset.
- `uv run dronefit render assets/fitted/target_v001/default.dronebank.json --seconds 20 --out renders/target_v001.wav` renders audio without needing the original sample.
- A standalone JUCE build produces audio from the same asset.
- A VST3 build loads in Ableton Live and responds to MIDI note-on/note-off.
- At least five parameters are exposed and automatable: Gain, Brightness, Motion Depth, Motion Rate, Roughness/Noise, plus Tune if time permits.
- The plugin opens with a custom GUI, not only JUCE GenericAudioProcessorEditor.
- The GUI has rotary knobs for the main synthesis parameters, visible labels, value readouts, and sections that make the model understandable.
- Moving a GUI knob updates the corresponding DAW parameter, and moving/automating the DAW parameter updates the GUI.
- pluginval passes at strictness level 5 before Ableton testing.
- No crashes when changing buffer size or sample rate.
- No raw user recordings are committed to git.

---

## 7. Offline fitting method: start simple and interpretable

### 7.1 Why not begin with a large neural network?

The user mentioned neural deconstruction into weights on basis functions. That is a good direction, but the first implementation should not begin with a black-box model. Drone sounds are often well represented by:

- slowly varying spectral peaks,
- inharmonic or harmonic oscillator banks,
- spectral tilt / formant-like envelopes,
- filtered noise bands,
- slow amplitude and frequency modulation.

A compact interpretable model will be easier to fit, easier to expose as DAW controls, and easier to render in a real-time plugin. Neural networks can be added later to infer the control trajectories or macro mappings.

### 7.2 Model family A: additive oscillator bank with slow basis controls

Approximate the target audio as:

```text
y_hat(t) = Σ_k a_k(t) * sin(φ_k(t)) + noise_residual(t)
φ_k(t) = φ_k(t-1) + 2π * f_k(t) / sample_rate
```

Where:

- `k` indexes partials / spectral peaks / oscillator components.
- `a_k(t)` is a non-negative amplitude envelope.
- `f_k(t)` is a slowly varying frequency.
- `a_k(t)` and `f_k(t)` are represented by low-dimensional basis functions rather than arbitrary per-sample curves.

Use low-rate control curves:

```text
control(t) = c0 + Σ_m [u_m * sin(2πmt/T) + v_m * cos(2πmt/T)]
```

or cubic-interpolated control points at, say, 10–100 Hz control rate.

Suggested first parameterization:

```text
log_amp_k(frame) = base_amp_k + B(frame) @ amp_weights_k
log_freq_ratio_k(frame) = B(frame) @ freq_weights_k
freq_k(frame) = base_freq_k * exp(log_freq_ratio_k(frame))
```

Clamp or regularize frequency modulation to remain musical and stable.

### 7.3 Model family B: spectral envelope + excitation

For drones with many partials or noisy/granular spectra, an oscillator for every peak may be inefficient. An alternative model:

- fixed oscillator/noise excitation;
- time-varying spectral envelope represented by low-rank basis functions;
- noise residual split into octave or Bark-like bands;
- macro controls warp the spectral envelope.

This may be useful after the additive MVP.

### 7.4 Model family C: DDSP-style neural control model

Later, add a small neural network that maps high-level macro controls or latent variables to oscillator/noise controls. This aligns with Differentiable Digital Signal Processing: classic DSP modules are differentiable and trained with backpropagation while staying interpretable.

Do not add this until the additive/basis export pipeline is working.

---

## 8. Offline fitting implementation plan

### 8.1 CLI commands to implement

Implement a `dronefit` CLI using Typer or argparse. Prefer Typer for readability.

Target commands:

```bash
uv run dronefit inspect data/raw/target.wav --out reports/target_inspect
uv run dronefit init-bank data/raw/target.wav --out runs/target_v001/init.dronebank.json --partials 96
uv run dronefit fit data/raw/target.wav --out assets/fitted/target_v001 --partials 96 --steps 5000
uv run dronefit render assets/fitted/target_v001/default.dronebank.json --seconds 20 --out renders/target_v001.wav
uv run dronefit compare data/raw/target.wav renders/target_v001.wav --out reports/target_compare_v001
uv run dronefit export-plugin-assets assets/fitted/target_v001 --out plugin/assets/default.dronebank.json
```

### 8.2 Audio preprocessing

Implement `python/dronefit/audio_io.py`:

- Load with `soundfile`.
- Convert integer PCM to float32 `[-1, 1]` if necessary.
- Optionally mono-sum for MVP.
- Remove DC offset.
- Normalize to a conservative peak such as `-6 dBFS` for fitting.
- Trim obvious silence only if requested; do not accidentally remove quiet drone tails.
- Resample if necessary using `torchaudio` or `librosa`/`soxr` depending on dependency choices.
- Save preprocessing metadata in the fitted asset.

### 8.3 Inspection plots and measurements

Implement `inspect` to generate:

- waveform plot;
- log-frequency spectrogram;
- linear-frequency spectrogram for low-frequency inspection;
- RMS over time;
- spectral centroid over time;
- spectral flatness over time;
- top spectral peaks averaged across time;
- optional estimated fundamental or dominant frequency, but do not assume harmonicity.

Use these to choose initial partial count and frequency range.

### 8.4 Initial spectral peak extraction

For the first version:

1. Compute STFT with a large FFT size, e.g. 8192 or 16384 at 48 kHz, depending on target length and frequency resolution.
2. Average magnitude over time or use median magnitude to ignore outlier events.
3. Find local peaks over a threshold.
4. Choose top `K` peaks by energy, with minimum spacing in Hz or cents to avoid duplicates.
5. Store `base_freq_k` and `base_amp_k`.

For harmonic drones, optionally estimate a base pitch and use harmonic partials. For inharmonic drones, use detected peaks directly.

### 8.5 PyTorch differentiable synthesis prototype

Implement `python/dronefit/synth.py`:

- `render_additive_bank(bank, seconds, sample_rate, device)`
- vectorized oscillator phase integration;
- envelope interpolation from control-rate frames to audio rate;
- optional filtered-noise bands later.

For long samples, rendering full audio for every optimization step may be expensive. Use random windows:

- choose 1–4 second windows;
- fit on random crops;
- keep control curves global or local-to-window with correct time offsets;
- periodically render the full sample for reporting.

### 8.6 Loss functions

Implement `python/dronefit/losses.py`:

```text
L_total = L_mrstft + λ_time * L_time + λ_amp_smooth * L_amp_smooth + λ_freq_smooth * L_freq_smooth + λ_energy * L_energy
```

Recommended first losses:

- multi-resolution STFT magnitude loss, e.g. FFT sizes `[512, 2048, 8192]`;
- log-magnitude STFT loss to capture quiet spectral components;
- time-domain L1 loss only at a low weight, because phase may be hard to match exactly;
- smoothness penalty on amplitude and frequency controls;
- frequency deviation penalty to keep partials near detected spectral peaks.

Avoid requiring exact waveform phase match for a drone unless the model is phase-initialized carefully. Perceptual spectral similarity is more important.

### 8.7 Fitting schedule

A robust first schedule:

1. Initialize base frequencies and amplitudes from spectral peaks.
2. Freeze frequencies; optimize amplitude basis weights for 500–1000 steps.
3. Unfreeze small frequency modulation; optimize amplitude + frequency controls.
4. Add noise-band residual if spectral loss shows broadband missing energy.
5. Export every N steps and render comparison audio.
6. Record metrics and plots to `reports/`.

### 8.8 Noise-band residual

After oscillator bank is working, model residual as filtered noise:

```text
residual(t) = Σ_b n_b(t) * bandpass_noise_b(t)
```

Where `n_b(t)` is a slow envelope. Use deterministic pseudo-random noise seeded in the plugin, not stored audio. Export band definitions and envelope/basis coefficients.

Possible bands:

- logarithmic bands from 20 Hz to 20 kHz;
- or bands based on residual spectrum peaks;
- or simple low/mid/high noise for MVP.

### 8.9 Export format

Start with JSON for readability. Move large arrays to binary later if needed.

Example `dronebank.v1`:

```json
{
  "schema": "dronebank.v1",
  "name": "target_v001",
  "created_by": "dronefit",
  "analysis_sample_rate": 48000,
  "render_reference_level_dbfs": -18.0,
  "model_type": "additive_fourier_controls",
  "duration_seconds": 20.0,
  "control_rate_hz": 50.0,
  "basis": {
    "type": "fourier",
    "order": 8,
    "period_seconds": 20.0
  },
  "partials": [
    {
      "freq_hz": 110.0,
      "phase_rad": 0.0,
      "amp_base": -4.2,
      "amp_coefficients": [0.0, 0.01, -0.02],
      "freq_log_ratio_coefficients": [0.0, 0.001, -0.001]
    }
  ],
  "noise_bands": [
    {
      "low_hz": 200.0,
      "high_hz": 800.0,
      "gain_base": -20.0,
      "gain_coefficients": [0.0, 0.1]
    }
  ],
  "macros": {
    "brightness": {
      "description": "Spectral tilt and upper partial scaling",
      "default": 0.5
    },
    "motionDepth": {
      "description": "Scales fitted amplitude/frequency modulation around base state",
      "default": 0.5
    }
  }
}
```

Use `pydantic` in Python to validate. Implement a matching lightweight C++ parser. For C++, JUCE has JSON utilities; use them for MVP. If performance or file size becomes an issue, switch to a compact binary format with a magic header and version.

---

## 9. Plugin design plan

### 9.1 VST3 plugin type

This should be a synth/instrument plugin:

- accepts MIDI input;
- produces audio output;
- no audio input required;
- format: VST3 first, Standalone target useful for debugging.

### 9.2 Synth behavior

MVP:

- Monophonic drone voice.
- MIDI note-on starts or retriggers the drone.
- MIDI note controls transposition relative to a root note, e.g. C3 or detected drone root.
- MIDI velocity controls gain and maybe brightness.
- Note-off either releases the envelope or latches depending on a `Latch` parameter.
- If no MIDI notes are active, either silence or sustain depending on `Drone Always On` / `Gate Mode` parameter.

Later:

- Polyphonic voices.
- MPE support.
- Per-note randomization.
- Multiple fitted banks and morphing.

### 9.3 Parameters to expose

Implement these first:

| Parameter | Range | Default | Meaning |
|---|---:|---:|---|
| Gain | -60 to +6 dB | -12 dB | Output gain |
| Tune | -24 to +24 semitones | 0 | Coarse transposition |
| Fine Tune | -100 to +100 cents | 0 | Fine pitch adjustment |
| Brightness | 0–1 | 0.5 | Spectral tilt / upper partial level |
| Motion Depth | 0–1.5 | 0.5 | Scales fitted modulation envelopes |
| Motion Rate | 0.05–4x | 1x | Speeds/slows modulation traversal |
| Roughness / Noise | 0–1 | 0.25 | Adds fitted noise-band residual or detune roughness |
| Detune / Spread | 0–1 | 0.1 | Slight oscillator detuning/stereo variation |
| Stereo Width | 0–1 | 0.5 | Width processing or partial panning spread |
| Attack | 0–10 s | 1 s | Amplitude attack |
| Release | 0–30 s | 3 s | Amplitude release |
| Root Note | MIDI 0–127 | 48 or detected | Reference pitch for transposition |

Parameter implementation notes:

- Use `AudioProcessorValueTreeState` or equivalent JUCE parameter management.
- Smooth all automatable parameters before using them in audio rendering.
- Never read UI state directly from the audio thread.
- Preserve parameter IDs forever once public, or handle compatibility migrations.

### 9.4 Real-time synthesis engine

Implement `SynthEngine` independent from JUCE UI:

```cpp
class SynthEngine {
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void loadBank(const DroneBank& bank);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void setParameters(const SynthParameters& params);
    void process(float** outputs, int numChannels, int numSamples);
    void reset();
};
```

Implementation details:

- Store partial frequencies, amplitudes, phases in contiguous vectors.
- Preallocate all buffers in `prepare` or `loadBank`.
- Use `juce::SmoothedValue` or custom smoothers for parameters.
- Use deterministic phase integration.
- Avoid denormals using `juce::ScopedNoDenormals` in `processBlock`.
- Cap partials above Nyquist and fade them out to avoid aliasing.
- For note transposition, multiply partial frequencies by `2^(semitones/12)`.
- For brightness, apply a spectral tilt around a pivot, e.g. 1 kHz.
- For motion rate/depth, evaluate basis/envelopes at a virtual time index.
- For stereo width, pan alternating partials or use equal-power panning by frequency group.

### 9.5 C++ DroneBank parser

Implement a small parser that:

- validates schema string;
- handles missing optional fields with defaults;
- clamps impossible values;
- reports user-facing errors without crashing;
- supports a built-in default bank compiled into the plugin;
- optionally supports external bank loading later.

### 9.6 GUI product requirement

The GUI is a required part of the instrument, not an afterthought. The user wants the plugin to feel like a small, focused synth instrument, with real knobs for the synthesis parameters, closer in spirit to premium synth collections than to a plain utility plugin.

Important boundary: use Arturia-style products only as a quality and interaction reference. Do not clone Arturia artwork, exact panel layouts, typography, colors, preset names, icons, knob caps, screenshots, or trade dress. Build an original interface that borrows broad ideas: tactile macro controls, clean grouping, pleasant contrast, clear labels, and a preset/bank area.

### 9.7 GUI visual direction

Start with a compact single-page editor. Suggested default size:

```text
960 x 600 px default
760 x 480 px minimum
1400 x 900 px maximum or resizable with aspect handling
```

Suggested layout:

```text
┌──────────────────────────────────────────────────────────────────────┐
│ DroneVST3        Bank: target_v001        Partials: 96   CPU/Status │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   MACRO CONTROLS                                                     │
│   [Gain] [Tune] [Brightness] [Motion Depth] [Motion Rate] [Roughness]│
│                                                                      │
│   SHAPE / SPACE                                                      │
│   [Detune] [Stereo Width] [Attack] [Release] [Root Note] [Latch]     │
│                                                                      │
│   PARTIAL / MOTION DISPLAY                                           │
│   subtle spectrum/partial display showing fitted peaks and movement  │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

Main visual priorities:

- Make the six macro knobs large and satisfying: Gain, Tune, Brightness, Motion Depth, Motion Rate, Roughness.
- Put envelope and stereo controls in a smaller secondary row.
- Show the loaded bank name and partial count so the user knows which fitted sound is active.
- Include a small partial/spectrum display if time permits; it should be decorative/informative, not required for audio correctness.
- Use a coherent visual language: dark or muted panel, subtle highlights, clear text, original knob design, and enough spacing to avoid a cramped developer-tool look.
- Support high-DPI displays and resizable plugin windows if feasible.

### 9.8 JUCE GUI architecture

Use a custom `AudioProcessorEditor`, not only `GenericAudioProcessorEditor`.

Recommended classes:

```text
plugin/Source/PluginEditor.h/.cpp
    Owns and lays out the top-level editor.

plugin/Source/UI/Theme.h/.cpp
    Central colors, fonts, spacing, sizes, corner radii.

plugin/Source/UI/DroneLookAndFeel.h/.cpp
    Custom JUCE LookAndFeel. Override rotary slider drawing, labels,
    buttons, combo boxes, and section panels.

plugin/Source/UI/Knob.h/.cpp
    Reusable labelled rotary knob component wrapping juce::Slider,
    juce::Label, and optional units/value display.

plugin/Source/UI/SectionPanel.h/.cpp
    Reusable titled panel background for groups such as Macro, Shape,
    Space, Envelope, Bank.

plugin/Source/UI/PartialDisplay.h/.cpp
    Lightweight visual display of partial frequencies/amplitudes and
    modulation position. Optional for the first GUI milestone.
```

JUCE implementation rules:

- Use `juce::Slider` with `SliderStyle::RotaryHorizontalVerticalDrag` for knobs.
- Attach sliders to parameters through `juce::AudioProcessorValueTreeState::SliderAttachment`.
- Keep each attachment alive for as long as the slider exists. Store attachments as members, not temporaries.
- Use `AudioProcessorValueTreeState` as the single source of truth for host-visible parameters.
- Never let the audio callback read GUI component state directly.
- Do all layout in `PluginEditor::resized()` using either manual `juce::Rectangle<int>` subdivision or JUCE `FlexBox`/`Grid`.
- Do all custom drawing in `paint()` or a custom `LookAndFeel`; avoid complicated GPU/OpenGL rendering for the first version.
- Use `juce::Timer` at 15–30 Hz for visual-only updates. Do not repaint at audio rate.
- Store plugin window size in the processor state if implementing resizable UI.

### 9.9 GUI-to-parameter binding pattern

The processor should expose an APVTS member or accessor:

```cpp
class DroneVST3AudioProcessor : public juce::AudioProcessor {
public:
    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }

private:
    juce::AudioProcessorValueTreeState apvts;
};
```

The editor should attach controls like this:

```cpp
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

class DroneVST3AudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
    explicit DroneVST3AudioProcessorEditor(DroneVST3AudioProcessor& processor);

private:
    DroneVST3AudioProcessor& processor;

    ui::DroneLookAndFeel lookAndFeel;

    ui::Knob gainKnob { "Gain", "dB" };
    ui::Knob tuneKnob { "Tune", "st" };
    ui::Knob brightnessKnob { "Brightness", "" };
    ui::Knob motionDepthKnob { "Motion", "" };
    ui::Knob motionRateKnob { "Rate", "x" };
    ui::Knob roughnessKnob { "Roughness", "" };

    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> tuneAttachment;
    std::unique_ptr<SliderAttachment> brightnessAttachment;
    std::unique_ptr<SliderAttachment> motionDepthAttachment;
    std::unique_ptr<SliderAttachment> motionRateAttachment;
    std::unique_ptr<SliderAttachment> roughnessAttachment;
};
```

Constructor pattern:

```cpp
auto& state = processor.getValueTreeState();

gainAttachment = std::make_unique<SliderAttachment>(state, ParameterIds::gain, gainKnob.slider());
tuneAttachment = std::make_unique<SliderAttachment>(state, ParameterIds::tune, tuneKnob.slider());
brightnessAttachment = std::make_unique<SliderAttachment>(state, ParameterIds::brightness, brightnessKnob.slider());
motionDepthAttachment = std::make_unique<SliderAttachment>(state, ParameterIds::motionDepth, motionDepthKnob.slider());
motionRateAttachment = std::make_unique<SliderAttachment>(state, ParameterIds::motionRate, motionRateKnob.slider());
roughnessAttachment = std::make_unique<SliderAttachment>(state, ParameterIds::roughness, roughnessKnob.slider());
```

`ParameterIds.h` should centralize stable string IDs:

```cpp
#pragma once

namespace ParameterIds {
static constexpr auto gain = "gain";
static constexpr auto tune = "tune";
static constexpr auto fineTune = "fineTune";
static constexpr auto brightness = "brightness";
static constexpr auto motionDepth = "motionDepth";
static constexpr auto motionRate = "motionRate";
static constexpr auto roughness = "roughness";
static constexpr auto detuneSpread = "detuneSpread";
static constexpr auto stereoWidth = "stereoWidth";
static constexpr auto attack = "attack";
static constexpr auto release = "release";
static constexpr auto rootNote = "rootNote";
static constexpr auto latch = "latch";
}
```

### 9.10 Reusable knob component

Implement `ui::Knob` as a small component that owns a `juce::Slider` and labels. The editor should not repeat boilerplate for every knob.

Shape:

```cpp
namespace ui {

class Knob final : public juce::Component {
public:
    Knob(juce::String labelText, juce::String unitText);

    juce::Slider& slider() noexcept { return slider_; }
    const juce::Slider& slider() const noexcept { return slider_; }

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    juce::Slider slider_;
    juce::Label nameLabel_;
    juce::Label valueLabel_;
    juce::String unitText_;
};

} // namespace ui
```

Constructor details:

- `slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);`
- `slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);`
- Use a custom value label, because it gives more control over formatting and avoids cramped default text boxes.
- Set sensible mouse drag sensitivity and double-click default values where useful.
- Add accessibility/name strings if straightforward.

### 9.11 Custom LookAndFeel

Implement an original vector-drawn knob first. Avoid bitmap filmstrip knobs until the parameter plumbing and layout are stable.

`DroneLookAndFeel` should override at least:

```cpp
void drawRotarySlider(juce::Graphics& g,
                      int x, int y, int width, int height,
                      float sliderPosProportional,
                      float rotaryStartAngle,
                      float rotaryEndAngle,
                      juce::Slider& slider) override;
```

Suggested drawing elements:

- knob body circle with subtle radial shading;
- outer value arc;
- tick mark or pointer line;
- small highlight that responds to enabled/disabled state;
- consistent color roles from `Theme`.

Keep this 2D and CPU-rendered using JUCE `Graphics`. Fancy animated lighting can be added later, but first aim for stable resizing, clear labels, and reliable host automation.

### 9.12 GUI sections and macro semantics

Group controls by musical meaning rather than implementation detail:

```text
Macro
  Gain, Brightness, Motion Depth, Motion Rate, Roughness

Pitch
  Tune, Fine Tune, Root Note

Shape / Space
  Detune, Stereo Width, Noise/Residual Mix

Envelope / Gate
  Attack, Release, Latch or Gate Mode

Bank
  Bank name, load button later, analysis sample rate, partial count
```

The GUI should help the user understand that the plugin is a learned drone model, not a normal subtractive synth. Use short, practical labels:

- **Brightness**: spectral tilt / upper partial energy.
- **Motion**: how much of the fitted modulation is active.
- **Rate**: how quickly the fitted modulation path is traversed.
- **Roughness**: noise residual, detune, or spectral instability.
- **Width**: stereo spread created from partial panning/phase.

### 9.13 Partial/spectrum display

A small display can make the instrument feel alive without becoming a full analyzer.

MVP display options:

- draw vertical lines for fitted partial frequencies with height based on amplitude;
- color or alpha-code partials by current brightness/motion influence;
- show a slow moving playhead for model-time traversal;
- show a gentle animated background shimmer based on `Motion Depth` and `Motion Rate`.

Real-time safety rules:

- The audio thread may write a tiny fixed-size snapshot into atomics or a lock-free single-producer/single-consumer buffer.
- The UI reads snapshots on a `Timer`; it must tolerate missed updates.
- Never allocate or lock in the audio thread for visualization.
- Do not run FFTs in the audio thread. If an FFT display is later desired, feed a preallocated FIFO and compute on the message thread or a worker thread.

### 9.14 GUI assets and design tooling

Start with vector drawing in code. This avoids asset-loading problems and scales cleanly on high-DPI displays.

Optional design tools:

- Figma, Penpot, Inkscape, Affinity Designer, or similar for mockups.
- Export SVG/PNG assets only when they genuinely improve the UI.
- Keep editable sources under `design/` and runtime assets under `plugin/assets/`.
- Do not commit commercial fonts unless their license explicitly allows redistribution. Prefer system fonts or open licensed fonts recorded in `docs/references.md`.
- Do not use screenshots or extracted assets from commercial plugins.

If image assets are used:

- embed them with `juce_add_binary_data`;
- load from JUCE `BinaryData` in the editor constructor or asset manager;
- avoid runtime file I/O for built-in skin assets;
- keep asset resolution reasonable to avoid bloating the VST3.

### 9.15 GUI milestone acceptance criteria

A successful first GUI milestone satisfies this:

- The editor is custom and visually distinct from JUCE defaults.
- At least eight rotary knobs are visible and usable.
- Knobs are attached to APVTS parameters and remain synchronized with host automation.
- Each knob has a label and value/unit display.
- The GUI shows bank name, model type, partial count, and plugin version.
- The editor can be opened/closed repeatedly in Standalone, pluginval, and Ableton without leaks or crashes.
- Resizing either works cleanly or is deliberately disabled with a polished fixed-size layout.
- GUI repainting does not cause audio glitches.

---

## 10. Build, validation, and Ableton testing

### 10.1 Local build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Find the built `.vst3` under the generated build folder. JUCE often creates plugin artifacts in a path under the target's build output.

### 10.2 Installing for Ableton Live

Use the correct VST3 folder for the OS.

Windows common VST3 system folder:

```text
C:\Program Files\Common Files\VST3
```

macOS system VST3 folder:

```text
/Library/Audio/Plug-Ins/VST3/
```

macOS user VST3 folder:

```text
~/Library/Audio/Plug-Ins/VST3/
```

During development, prefer a custom VST3 folder if it avoids admin permissions, but keep VST2 and VST3 folders separate. In Live, enable VST3 sources and rescan.

### 10.3 pluginval

Install pluginval or build it from source. Run headless validation before opening in Ableton:

```bash
pluginval --strictness-level 5 path/to/DroneVST3.vst3
```

If it fails, fix pluginval issues before diagnosing Ableton-specific behavior. Then try strictness 7–10 later.

### 10.4 Ableton smoke test

1. Copy/install the `.vst3`.
2. Launch Ableton Live.
3. Enable VST3 plug-in system folder or custom folder.
4. Rescan.
5. Insert plugin on a MIDI track.
6. Arm track or create MIDI clip.
7. Play sustained notes.
8. Automate Brightness, Motion Depth, Motion Rate, and Roughness.
9. Change sample rate / buffer size if possible and confirm stability.
10. Save and reload the Live set to test plugin state restoration.

---

## 11. Development milestones

### Milestone 0: Repo and environment

Deliverables:

- Git repo initialized.
- uv environment working.
- PyTorch verified CPU/GPU.
- JUCE submodule present.
- CMake config created.
- Empty plugin builds.

Acceptance:

- `uv run python -c "import torch"` succeeds.
- `cmake --build build` succeeds.

### Milestone 1: Audio inspection CLI

Deliverables:

- `dronefit inspect` command.
- Feature plots and spectrograms.
- Metadata JSON report.

Acceptance:

- Works on a synthetic test file.
- Works on user-provided target file without committing it.

### Milestone 2: Non-neural spectral initialization

Deliverables:

- STFT peak extraction.
- Initial `.dronebank.json` with base partials.
- Python renderer for oscillator bank.

Acceptance:

- `dronefit render` produces a stable drone-like WAV from the initial bank.

### Milestone 3: PyTorch fitting

Deliverables:

- Differentiable additive renderer.
- Multi-resolution STFT loss.
- Optimization loop.
- Rendered comparisons every N steps.

Acceptance:

- Spectral loss decreases.
- Rendered WAV is audibly closer than initialization.
- Exported bank remains compact.

### Milestone 4: C++ synth engine reads exported bank

Deliverables:

- `DroneBank` parser.
- `SynthEngine` renders the bank in standalone plugin.
- Basic parameters mapped.

Acceptance:

- Standalone app produces audio with no Python.
- No heap allocation in audio process path after prepare/load.

### Milestone 4.5: Custom GUI shell and knob binding

Deliverables:

- Custom `AudioProcessorEditor` replaces any generic editor.
- Reusable `Knob`, `SectionPanel`, `Theme`, and `DroneLookAndFeel` classes exist.
- Main parameters are visible as rotary knobs.
- Slider attachments bind GUI controls to APVTS parameters.
- Bank/status labels show fitted asset metadata.

Acceptance:

- Moving knobs changes sound through the same parameter path used by host automation.
- Host automation updates the knob positions.
- Opening/closing the editor repeatedly does not crash.
- GUI repaint load is modest and does not glitch audio.

### Milestone 5: VST3 validation and Ableton load

Deliverables:

- VST3 build.
- pluginval pass at strictness 5.
- Ableton load test.

Acceptance:

- VST3 appears in Ableton.
- MIDI note creates audio.
- DAW automation moves parameters without crashes/clicks.

### Milestone 6: Improve model quality

Possible tasks:

- Add noise-band residual.
- Add stereo partial panning.
- Add spectral tilt macro.
- Add modulation random seed.
- Fit multiple target clips and morph between banks.
- Add ONNX/LibTorch only if a learned runtime model becomes necessary.

---

## 12. Coding standards and tests

### 12.1 Python standards

- Type-hint public functions.
- Use `ruff` for linting.
- Use `pytest` for tests.
- Keep CLI code thin; put logic in importable modules.
- Save all fitting configuration into output directories.
- Make results reproducible with seeds.

Suggested commands:

```bash
uv run ruff check .
uv run pytest
```

### 12.2 C++ standards

- C++20.
- RAII ownership.
- Avoid raw owning pointers.
- Audio thread must be real-time safe.
- No exceptions crossing audio callback boundaries.
- Use assertions in debug, graceful failure in release.
- Prefer simple data-oriented vectors for partial rendering.

### 12.3 Tests to write early

Python:

- schema roundtrip test;
- render output shape and no NaN test;
- loss function finite test;
- fitting smoke test on a generated synthetic drone.

C++:

- parse minimal bank;
- reject invalid schema;
- render silence when no note active;
- render nonzero audio after note-on;
- no NaNs in output buffer.

---

## 13. Synthetic fixture for tests

Create a tiny synthetic drone generator so tests do not require private user audio:

```python
import numpy as np
import soundfile as sf

sr = 48000
seconds = 4
t = np.arange(sr * seconds) / sr
base = 110.0
motion = 0.003 * np.sin(2 * np.pi * 0.17 * t)
y = np.zeros_like(t)
for k, amp in [(1, 0.4), (2.01, 0.2), (3.98, 0.12), (7.2, 0.05)]:
    freq = base * k * (1.0 + motion)
    phase = 2 * np.pi * np.cumsum(freq) / sr
    y += amp * np.sin(phase)
y += 0.02 * np.random.default_rng(0).standard_normal(len(t))
y /= max(1e-9, np.max(np.abs(y)))
sf.write("data/examples/synthetic_drone.wav", y.astype(np.float32), sr)
```

Commit only small synthetic fixtures, not user recordings.

---

## 14. Notes on neural-network options

Use neural methods as accelerators or macro mappers, not as the first real-time dependency.

Possible later approaches:

1. **Neural control estimator:** Train a small network to infer additive/noise controls from STFT features. Export controls, not the network.
2. **Latent macro model:** Fit several variations/clips and train a small MLP that maps macro controls to bank coefficients. Consider ONNX only if runtime inference is truly needed.
3. **Autoencoder/VAE:** Learn a low-dimensional latent over spectral frames, then decode to oscillator/noise controls. More flexible but harder to make stable and real-time safe.
4. **DDSP-style model:** Neural network outputs interpretable DSP parameters. Strong candidate after the baseline works.

If runtime inference is later required:

- Prefer ONNX Runtime C++ over embedding Python.
- Benchmark inference outside the audio thread first.
- Consider precomputing control curves at low rate on a worker thread and crossfading into audio thread buffers.
- Never run unpredictable dynamic allocation or blocking calls inside the audio callback.

---

## 15. Design decisions to record in `docs/decision-log.md`

Start the decision log with entries like:

```markdown
# Decision Log

## 2026-06-02: Use JUCE for VST3 plugin shell

Decision: Use JUCE + CMake for the VST3 instrument rather than direct Steinberg SDK.

Rationale: Faster cross-platform plugin development, built-in parameter/state/UI/audio utilities, VST3 and Standalone targets from one project.

Consequences: Must comply with JUCE licensing. Keep plugin code structured so lower-level synthesis is not tightly coupled to JUCE.

## 2026-06-02: Keep PyTorch offline only for MVP

Decision: Use PyTorch for fitting/export, but not inside plugin runtime.

Rationale: Real-time safety, simpler distribution, smaller plugin, fewer platform-specific dynamic library issues.

Consequences: Plugin must render from exported coefficients. Neural runtime can be revisited later.

## 2026-06-02: Treat the GUI as a first-class deliverable

Decision: Build a custom JUCE GUI with rotary knobs and an original premium-synth-inspired look rather than relying on `GenericAudioProcessorEditor`.

Rationale: The instrument should feel playable and inviting inside Ableton. The synthesis model exposes meaningful macro controls, so the UI should make those controls tactile and understandable.

Consequences: Add UI components, theme/look-and-feel code, APVTS slider attachments, GUI tests/smoke checks, and original design assets. Do not copy commercial plugin artwork or trade dress.
```

---

## 16. README skeleton

Use this as the first README:

```markdown
# DroneVST3

A prototype VST3 instrument that fits a compact additive/spectral synthesis model to a user-provided drone sample, then renders it in a real-time JUCE plugin for Ableton Live.

## Status

Prototype. Not ready for distribution.

## Layout

- `python/dronefit/`: offline analysis, fitting, and export tools.
- `plugin/`: JUCE VST3/Standalone plugin source.
- `assets/fitted/`: compact fitted model assets.
- `data/raw/`: private user audio, not tracked by git.

## Quick start

```bash
uv sync
uv run dronefit --help
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

## Safety

Do not commit private recordings or large fitting outputs.
```

---

## 17. Concrete next action list for CODEX

1. Create repo bookkeeping and commit it.
2. Set up uv and baseline Python dependencies.
3. Verify PyTorch CPU/GPU availability.
4. Add JUCE submodule and minimal CMake plugin.
5. Build a silent VST3/Standalone plugin.
6. Add a trivial sine/drone synth engine and confirm audio in Standalone.
7. Replace any generic editor with a custom JUCE editor shell.
8. Implement reusable knob, theme, section panel, and LookAndFeel classes.
9. Bind at least eight GUI knobs to APVTS parameters with SliderAttachments.
10. Implement `dronefit inspect`.
11. Implement synthetic fixture generation and tests.
12. Implement spectral peak extraction and JSON bank export.
13. Implement Python renderer and compare render plots.
14. Implement PyTorch fitting loop.
15. Implement C++ `DroneBank` parser and additive bank renderer.
16. Wire parameters, smoothing, GUI labels, and bank metadata display.
17. Run pluginval.
18. Test in Ableton Live.

---

## 18. Pitfalls to avoid

- Do not start by building a giant neural audio model. Get the interpretable oscillator/noise baseline working first.
- Do not embed Python in the plugin.
- Do not call PyTorch from `processBlock`.
- Do not allocate memory in the audio callback.
- Do not do file I/O in the audio callback.
- Do not assume the drone has a single fundamental frequency.
- Do not optimize for exact time-domain sample matching too early; spectral/perceptual similarity matters more.
- Do not commit raw recordings.
- Do not use a VST2 folder for VST3 or mix plugin formats in one folder.
- Do not ignore plugin state restoration; Ableton project reload must preserve parameters and selected bank.
- Do not ship only the generic JUCE parameter editor; the custom knob GUI is part of the product goal.
- Do not copy Arturia or other commercial plugin artwork, screenshots, exact layouts, fonts, icons, knob caps, or branding.
- Do not make the GUI read or write audio-thread internals directly. Use parameters and safe snapshots.
- Do not rely only on Ableton testing; run pluginval first.

---

## 19. Useful implementation details

### 19.1 Parameter smoothing

For each block:

1. Read atomic parameter values from JUCE parameter state.
2. Update smoothers.
3. Use per-sample or per-small-chunk smoothed values for sensitive parameters like gain, tune, brightness, and motion depth.

Pitch/tune changes can click if abrupt. Smooth pitch ratios over 5–50 ms unless intentional.

### 19.2 Avoiding aliasing

For every partial:

```text
if freq >= 0.48 * sample_rate:
    skip or fade out partial
```

When transposing upward, many fitted high partials may exceed Nyquist. Fade them instead of hard switching if possible.

### 19.3 Stereo strategy

MVP stereo options:

- duplicate mono to stereo;
- pan partials by frequency or index;
- add tiny deterministic phase offsets per channel;
- use width parameter to interpolate mono ↔ widened stereo.

Do not introduce random unstable phase every block.

### 19.4 Motion traversal

Maintain a `modelTimeSeconds` that advances by:

```text
modelTime += hostDeltaTime * motionRate
```

Wrap around the fitted duration for looped modulation. Crossfade near wrap point if needed. `Motion Depth = 0` should freeze near the base/static fitted state.

### 19.5 Macro mappings

Brightness example:

```text
amp_k *= exp(brightnessAmount * log(freq_k / pivotHz))
```

Clamp to avoid extreme high-frequency gain.

Roughness example:

- increase detune spread between paired oscillators;
- increase noise-band gain;
- slightly increase frequency modulation depth.

Motion Depth example:

```text
amp_mod = base_amp + motionDepth * fitted_amp_deviation
freq_mod = base_freq * exp(motionDepth * fitted_log_freq_deviation)
```

### 19.6 GUI repaint and automation behavior

Expected behavior inside Ableton:

- Dragging a knob should create/change the host parameter value, not a private UI-only value.
- Ableton automation should visibly move the knob during playback.
- The plugin should save and reload all parameter values with the Live set.
- Double-click or modifier-click reset behavior is useful, but only if it does not conflict with host conventions.
- Fine adjustment via Shift-drag or JUCE slider velocity settings is desirable for Tune, Fine Tune, Gain, and Motion Rate.

Implementation notes:

- Use parameter normalisable ranges and text conversion lambdas so host automation names and values are readable.
- Keep display formatting consistent between host text and GUI labels where practical.
- Use `setBufferedToImage(true)` cautiously only for components that are expensive to repaint and static; do not cache frequently animated components blindly.
- A `PartialDisplay` should repaint on a timer, not in response to every processed block.
- Profile GUI CPU if Ableton feels sluggish with the editor open.

---

## 20. Final note for CODEX

Prioritize a working vertical slice over perfect sound quality:

```text
sample -> inspect -> initialize bank -> render approximate WAV -> fit better bank -> load in plugin -> play in Ableton -> automate parameters
```

Once that loop exists, improving the fitting model becomes much easier. The project should remain split into clean layers:

- offline analysis/fitting/export;
- portable model asset;
- real-time C++ synthesis;
- JUCE plugin wrapper/UI.

