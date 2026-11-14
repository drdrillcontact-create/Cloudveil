# Cloudveil — Dreamy Shimmer Reverb (VST3 & AU)

Cross-platform JUCE 9 audio effect plugin compiled via CMake and GitHub Actions.

The repository builds a Windows x64 VST3 and universal macOS AU/VST3 (Apple Silicon + Intel).

## The Plugin
- **Reverb:** an eight-line feedback delay network with Decay, Size, Pre-Delay, Drift (slow
  movement), Warmth (damping) and Width.
- **Shimmer:** the reverb, one octave up, fed back into itself. The octave is made on the
  spectrum, so a held note gets its octave in tune and at a steady level, and it is added on top
  of the tail rather than taken out of it.
- **Freeze:** holds the cloud that is ringing for as long as it is switched on.
- **50 presets** in seven categories; the arrows next to the preset name step through them, a
  click on the name opens the list.
- **Display:** three ribbons that follow the reverb: left to right is low to high, they swell
  where the sound's energy is, and the third one follows the shimmer.
- **Window:** drag the bottom right corner to resize it (570 to 2280 pixels wide). The size is
  saved with the project.

## Build on GitHub Actions
1. Push this repository to GitHub (or trigger **Actions -> Build Cloudveil Reverb -> Run workflow**).
2. Download the compiled `.zip` artifacts from the workflow run summary:
   - `Cloudveil-Windows-x64-VST3.zip`
   - `Cloudveil-macOS-Universal-AU-VST3.zip`

Every run also checks what it built, and only uploads the artifacts if all of it passes:
- the reverb engine on its own (`Tests/EngineChecks.cpp`), as compiled for each platform,
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

`Tests/run-engine-checks.sh` checks the reverb engine by itself: that the shimmer turns a note
into its octave, in tune and at a steady level; that more shimmer never shortens the tail; that a
frozen cloud keeps its level; that the engine survives garbage input and odd sample rates. It
needs a C++17 compiler and nothing else, and takes about a minute. CMake builds the same checks
as `CloudveilEngineChecks`.

`Tests/run-harness.sh` runs the real processor and editor without a host or a display and checks
what hosts and users rely on: value text, presets, state recall, tail length, bypass and reset,
channel layouts, the preset arrows and list, window sizes, and that the display follows the
sound. It needs Linux with `git` and `g++` but no CMake (tested on Debian 12, x86-64); the first
run fetches and compiles JUCE, which takes about three minutes.

```bash
Tests/run-engine-checks.sh          # the engine
Tests/run-harness.sh                # processor and editor
Tests/run-harness.sh editor.png     # ...and save pictures of the editor (editor-1-at-rest.png, ...)
```

## Repository Layout

- `Source/DreamyReverbDSP.h` — reverb and shimmer DSP engine (no JUCE in it)
- `Source/PluginProcessor.*` — JUCE audio processor, parameters, presets, and state
- `Source/PluginEditor.*` — plugin interface: controls, preset list, display, look and feel
- `.github/workflows/build.yml` — Windows and macOS CI builds and validation
- `.github/scripts/` — helpers for the workflow (engine checks, pluginval runs, notes on the run page)
- `Tests/` — checks for the engine, and for the processor and editor
- `CMakeLists.txt` — CMake/JUCE project configuration
