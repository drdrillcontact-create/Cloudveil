# Cloudveil — Dreamy Shimmer Reverb (VST3 & AU)

Cross-platform JUCE 9 audio effect plugin compiled via CMake and GitHub Actions.

The repository builds a Windows x64 VST3 and universal macOS AU/VST3 (Apple Silicon + Intel).

## Build on GitHub Actions
1. Push this repository to GitHub (or trigger **Actions -> Build Cloudveil Reverb -> Run workflow**).
2. Download the compiled `.zip` artifacts from the workflow run summary:
   - `Cloudveil-Windows-x64-VST3.zip`
   - `Cloudveil-macOS-Universal-AU-VST3.zip`

Every run also validates what it built, and only uploads the artifacts if that passes:
- both VST3s with [pluginval](https://github.com/Tracktion/pluginval) at strictness level 10,
- the Audio Unit with Apple's `auval`.

The verdicts are shown as notes on the run page. pluginval tries the plug-in with random values;
the workflow gives it fixed seeds (`PLUGINVAL_SEEDS` in `build.yml`) so that a run is repeatable.
**Run workflow** accepts other seeds, and a failure names the seed it happened with.

## Installing the Plugins
- **Windows (64-bit VST3):** Copy `Cloudveil.vst3` into `C:\Program Files\Common Files\VST3\`
- **macOS (AU & VST3):**
  - Copy `Cloudveil.component` into `~/Library/Audio/Plug-Ins/Components/`
  - Copy `Cloudveil.vst3` into `~/Library/Audio/Plug-Ins/VST3/`
  - Since GitHub Actions artifacts are ad-hoc signed (not Apple-notarized), clear macOS quarantine once after copying:
    ```bash
    xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Cloudveil.component
    xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Cloudveil.vst3
    ```

## Licensing Note
This project fetches **JUCE 9.0.5** via CMake `FetchContent`. Use of JUCE is governed by the JUCE 9 End User License Agreement or AGPLv3.

## Local Build

JUCE 9.0.5 is fetched automatically by CMake.

```bash
cmake -S . -B build
cmake --build build --config Release
```

On macOS, build both supported architectures and formats:

```bash
cmake -S . -B build -G Xcode \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release
```

## Tests

`Tests/run-harness.sh` runs the real processor and editor without a host or a display and checks
what hosts rely on: value text, programs, state recall, tail length, bypass and reset, channel
layouts. It needs Linux with `git` and `g++` but no CMake (tested on Debian 12, x86-64); the first
run fetches and compiles JUCE, which takes about two minutes.

```bash
Tests/run-harness.sh                # run the checks
Tests/run-harness.sh editor.png     # ...and save a picture of the editor
```

## Repository Layout

- `Source/DreamyReverbDSP.h` — reverb and shimmer DSP engine
- `Source/PluginProcessor.*` — JUCE audio processor, parameters, presets, and state
- `Source/PluginEditor.*` — plugin interface and custom LookAndFeel
- `.github/workflows/build.yml` — Windows and macOS CI builds and validation
- `.github/scripts/` — helpers for the workflow (pluginval runs, notes on the run page)
- `Tests/` — host-behaviour checks for the processor and editor
- `CMakeLists.txt` — CMake/JUCE project configuration
