# D.A LUNE — Dreamy Shimmer Reverb (VST3 & AU)

A reverb plug-in by D.A Production, built with JUCE 9 through CMake and GitHub Actions:

- **Windows:** 64-bit VST3, and `D.A LUNE Installer.exe`
- **macOS:** Audio Unit and VST3, universal (Apple Silicon + Intel), and `D.A LUNE Installer.app`

("Cloudveil" is the project's working name: the repository, the build targets and the classes
still carry it. Up to version 1.1 it was also the plug-in's name.)

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

## Installing with the Installer

Download the installer for your system from a workflow run (see below), unzip it and start it.
It shows a welcome page, then the folder it installs into (ticked; untick what you do not want),
then copies the plug-in and says when it is done: **Next → Next → Finish**. Its window can be
resized by dragging any edge or corner, and moved by dragging it anywhere that is not a button.

| System  | Download                         | Installs                                                                                        |
|---------|----------------------------------|-------------------------------------------------------------------------------------------------|
| Windows | `D.A-LUNE-Windows-Installer.zip` | `D.A LUNE.vst3` into `C:\Program Files\Common Files\VST3`                                       |
| macOS   | `D.A-LUNE-macOS-Installer.zip`   | `D.A LUNE.component` into `~/Library/Audio/Plug-Ins/Components`, `D.A LUNE.vst3` into `~/Library/Audio/Plug-Ins/VST3` |

An older copy of D.A LUNE in the same folder is replaced. Close your DAW first: a plug-in that a
running host has open cannot be replaced, and the installer then says so. Afterwards, let the
DAW rescan its plug-ins if it does not list D.A LUNE by itself.

**The installers are not signed with a publisher's certificate**, so both systems ask before
running them:

- **Windows** shows "Windows protected your PC": choose **More info → Run anyway**. It then asks
  for administrator rights ("unknown publisher"), which the installer needs to write into
  `Program Files`.
- **macOS** refuses the first double-click. On macOS 14 and earlier, right-click the installer
  and choose **Open**, then **Open** again. On macOS 15 and later, try to open it once, then go
  to **System Settings → Privacy & Security** and choose **Open Anyway**. It installs into your
  own Library folder and needs no password.

**Coming from Cloudveil 1.1 or earlier:** with the new name the plug-in has a new identity, so a
host sees D.A LUNE as a different plug-in. Projects made with Cloudveil keep looking for
Cloudveil, and the installer does not remove it: delete `Cloudveil.vst3` / `Cloudveil.component`
by hand once you no longer need them.

The installer can also be run without its window: `"D.A LUNE Installer" --silent` installs into
the standard folders and ends with exit code 0 if that worked (`--vst3-dir <folder>` and
`--au-dir <folder>` choose other folders, `--log <file>` writes down what happened).

## Installing by Hand
The plug-ins are also available without the installer (`D.A-LUNE-Windows-x64-VST3.zip`,
`D.A-LUNE-macOS-Universal-AU-VST3.zip`):

- **Windows (64-bit VST3):** Copy `D.A LUNE.vst3` into `C:\Program Files\Common Files\VST3\`
- **macOS (AU & VST3):**
  - Copy `D.A LUNE.component` into `~/Library/Audio/Plug-Ins/Components/`
  - Copy `D.A LUNE.vst3` into `~/Library/Audio/Plug-Ins/VST3/`
  - The builds are ad-hoc signed (not Apple-notarized), so clear macOS quarantine once after copying:
    ```bash
    xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"D.A LUNE.component"
    xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"D.A LUNE.vst3"
    ```

## Build on GitHub Actions
1. Push this repository to GitHub (or trigger **Actions -> Build D.A LUNE -> Run workflow**).
2. Download the artifacts from the workflow run summary:
   - `D.A-LUNE-Windows-Installer.zip`, `D.A-LUNE-macOS-Installer.zip` — the installers
   - `D.A-LUNE-Windows-x64-VST3.zip`, `D.A-LUNE-macOS-Universal-AU-VST3.zip` — the plug-ins on their own
   - `D.A-LUNE-Windows-installer-pictures.zip`, `D.A-LUNE-macOS-installer-pictures.zip` — what the
     installer looked like on the build machine: screenshots, and each page as it was drawn

Every run also checks what it built, and only uploads the installers and plug-ins if all of it passes:
- the reverb engine on its own (`Tests/EngineChecks.cpp`), as compiled for each platform,
- both VST3s with [pluginval](https://github.com/Tracktion/pluginval) at strictness level 10,
- the installer: it is started on the build machine and goes through itself (`--self-test`). Where
  the system lets it, it uses the real mouse: it takes its window by a corner and drags it to
  another size, then clicks Next, Next and Finish. What it installed into the standard folders is
  compared, file for file, with what was built. Then the installer is unpacked from the zip that
  is uploaded and run once more with `--silent`, which replaces the copy that is already there,
- the Audio Unit with Apple's `auval`, in the place the installer put it.

The verdicts are shown as notes on the run page. pluginval tries the plug-in with random values;
the workflow gives it fixed seeds (`PLUGINVAL_SEEDS` in `build.yml`) so that a run is repeatable.
**Run workflow** accepts other seeds, and a failure names the seed it happened with.

## Local Build

JUCE 9.0.5 is fetched automatically by CMake. The build makes the plug-in and, with the plug-in
inside it, the installer.

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

The plug-ins are then in `build/Cloudveil_artefacts/Release/`, the installer in
`build/CloudveilInstaller_artefacts/Release/`. The names, the company and the version are set at
the top of `CMakeLists.txt`.

## Tests

`Tests/run-engine-checks.sh` checks the reverb engine by itself: that the shimmer turns a note
into its octave, in tune and at a steady level; that more shimmer never shortens the tail; that a
frozen cloud keeps its level; that the engine survives garbage input and odd sample rates. It
needs a C++17 compiler and nothing else, and takes about a minute. CMake builds the same checks
as `CloudveilEngineChecks`.

`Tests/run-harness.sh` runs the real processor and editor without a host or a display and checks
what hosts and users rely on: value text, presets, state recall, tail length, bypass and reset,
channel layouts, the preset arrows and list, window sizes, and that the display follows the
sound.

`Tests/run-installer-harness.sh` does the same for the installer: its routine that puts the
plug-in in place (into temporary folders: replacing an older copy, left-overs of an interrupted
installation, a folder it may not write to, a damaged payload), its pages, clicked through, and
its window (where the mouse finds the edges that resize it, the proportions it keeps).

The last two need Linux with `git` and `g++` but no CMake (tested on Debian 12, x86-64); the
first run fetches and compiles JUCE, which takes about three minutes.

```bash
Tests/run-engine-checks.sh                   # the engine
Tests/run-harness.sh                         # processor and editor
Tests/run-harness.sh editor.png              # ...and save pictures of the editor (editor-1-at-rest.png, ...)
Tests/run-installer-harness.sh               # the installer
Tests/run-installer-harness.sh pictures      # ...and save a picture of every page into pictures/
```

## Repository Layout

- `Source/DreamyReverbDSP.h` — reverb and shimmer DSP engine (no JUCE in it)
- `Source/PluginProcessor.*` — JUCE audio processor, parameters, presets, and state
- `Source/PluginEditor.*` — plugin interface: controls, preset list, display, look and feel
- `Installer/Source/` — the installer: `InstallerEngine.*` puts the plug-in in place,
  `InstallerView.*` draws the pages, `InstallerWindow.h` is its window, `InstallerMain.cpp` the
  program and its self-test
- `Installer/Assets/` — the installer's picture, typefaces and icon
- `.github/workflows/build.yml` — Windows and macOS CI builds and validation
- `.github/scripts/` — helpers for the workflow (engine checks, pluginval runs, running the
  installer, notes on the run page)
- `Tests/` — checks for the engine, for the processor and editor, and for the installer
- `CMakeLists.txt` — CMake/JUCE project configuration

## Licensing Note
This project fetches **JUCE 9.0.5** via CMake `FetchContent`. Use of JUCE is governed by the JUCE 9 End User License Agreement or AGPLv3.

The installer's typefaces, Red Hat Display and Orbitron, are used under the SIL Open Font
License 1.1 (`Installer/Assets/RedHatDisplay-OFL.txt`, `Installer/Assets/Orbitron-OFL.txt`).
