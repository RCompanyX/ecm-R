# Building ECM-R

## Overview

This repository does **not** build the ECM-R game plugin through CMake.
The `.asi` module is generated from a Premake script located at `lua/windows.lua`, and project files are created with `generate.bat`.

For **Need for Speed: Underground 2**, the relevant and maintained target is the **32-bit** build.

The generated workspace may expose additional platforms such as `Win-x64`, but repository validation, deployment guidance, and runtime naming are currently centered on `Release | Win-x86`.

## CI Builds (GitHub Actions)

Every push and pull request automatically compiles `Release | Win-x86` via GitHub Actions.
A manually triggered `Debug | Win-x86` workflow is also available; it verifies and uploads the DLL, ASI, and PDB as the `ecm-r-debug-build` artifact.
Pre-built artifacts are available without a local Windows toolchain:

1. Go to the [Actions](https://github.com/RCompanyX/ecm-r/actions) tab.
2. Open the latest successful workflow run.
3. Download the `ecm-r-build` artifact (ZIP file).
4. Extract `ecm-r.x86.asi` and the `ecm-r/translations` folder into your game's `scripts` folder alongside `ecm-r.x86.ini` and `bass.dll`.

This is useful for macOS/Linux users who cannot run the Visual Studio toolchain locally.

## Requirements

- Windows _(required only for local builds — see [CI Builds](#ci-builds-github-actions) for a no-toolchain alternative)_
- Git
- Visual Studio with C++ desktop build tools
- MSVC **v143** toolset (the generated projects target Visual Studio 2022 tools by default)
- A Windows SDK installed through Visual Studio

### Repository-managed dependencies

This repository already includes or references the main dependencies needed for compilation:

- `tools/premake5.exe`
- `deps/freetype-2.12.1`
- Git submodules:
  - `deps/imgui`
  - `deps/ini_rw`
  - `deps/minhook`
  - `deps/kiero`

## 1. Clone the repository

```powershell
git clone https://github.com/RCompanyX/ecm-r.git
cd ecm-r
```

If the repository was already cloned without submodules, initialize them before generating the solution:

```powershell
git submodule update --init --recursive
```

## 2. Generate the Visual Studio solution

From the repository root, run:

```powershell
.\generate.bat
```

This does two things:

1. Updates the required submodules.
2. Generates `build/ECM-R.sln` using `tools/premake5.exe` and `lua/windows.lua`.

## 3. Build the plugin in Visual Studio

1. Open `build/ECM-R.sln`.
2. Select the solution configuration:
   - `Release`
3. Select the solution platform:
   - `Win-x86`
4. Build the `Overlay` project, or build the full solution.

### Expected output

For the 32-bit release build, the generated files are placed in:

- `build/bin/Release-Win-x86/x86/ecm-r.x86.dll`
- `build/bin/Release-Win-x86/x86/ecm-r.x86.asi`
- `build/bin/Release-Win-x86/x86/ecm-r/translations/en.ini`
- `build/bin/Release-Win-x86/x86/ecm-r/translations/es.ini`

## 4. Runtime package

The build produces the DLL and also creates an `.asi` copy during post-build.

The final runtime package should include at least:

- `ecm-r.x86.asi`
- `ecm-r.x86.ini` (or allow ECM-R to create it on first launch)
- `bass.dll` downloaded from the official BASS website
- `ecm-r/translations/en.ini` and `ecm-r/translations/es.ini`

This project loads the native BASS runtime dynamically, and `bass.dll` is required at runtime. Users should obtain it from the official BASS website:

- https://www.un4seen.com/

On the Un4seen website, open the `BASS` page, download the Windows 32-bit (Win32) ZIP package, and copy the `bass.dll` file from the root of that ZIP into the same folder as the ECM-R runtime files.

The current runtime integration has been tested with BASS `v2.4.18.11`.
ECM-R does not bundle or redistribute `bass.dll`; users must obtain the official runtime themselves.
BASS remains subject to the official BASS license terms, and commercial usage may require a separate licence.

## 5. Optional command-line build

After generating the solution, the 32-bit release target can also be built with MSBuild:

```powershell
msbuild build\ECM-R.sln /t:Overlay /p:Configuration=Release /p:Platform=Win-x86 /m
```

## 6. Install into the game

Copy the runtime files into the location used by your ASI loader or mod manager.
For a typical setup, this means placing:

- `ecm-r.x86.asi`
- `ecm-r.x86.ini`
- `bass.dll` obtained from the official BASS website
- `ecm-r/translations/en.ini` and `ecm-r/translations/es.ini`

inside the mod loader's expected `scripts` directory, with `bass.dll` placed next to the ECM-R runtime files. Use the `bass.dll` found in the root of the official Windows 32-bit BASS ZIP package from the `BASS` page on https://www.un4seen.com/.

Then create a `Music` folder in the expected mod location and place the audio files there.

On first launch, ECM-R also creates `ecm-r.x86.ini` automatically. The generated configuration now includes a `[config]` section with:

```ini
[config]
shuffle_enabled = true
repeat_enabled = true
stop_music_on_loading_screens = true
ingame_movie_muting = true
language = en
```

These options control playlist playback behavior and loading screen handling:

- `shuffle_enabled = true`: play valid tracks in shuffled order.
- `repeat_enabled = true`: restart the valid track list after the last song finishes.
- `stop_music_on_loading_screens = true`: stop custom music during loading screens.
- `ingame_movie_muting = true`: mute custom music during supported in-game movie packages. This is a normal `[config]` option; the retained Experimental entry point is empty and unused.
- `language = en`: use English overlay text; choose `es` for neutral Spanish. External bundles are loaded once at startup.

The default values are enabled to preserve the expected ECM-R playback experience.

## Troubleshooting

### MSB8020: Visual Studio 2022 build tools not found

If the build reports that toolset `v143` is missing, install the Visual Studio 2022 C++ build tools.
The generated solution currently targets `v143` by default.

If a newer Visual Studio version is installed, it can still open the solution, but `v143` must be available unless the projects are manually retargeted.

### CMake cannot build the plugin

Even if the workspace is opened as a CMake-capable folder, the plugin itself is not configured through a top-level `CMakeLists.txt`.
Use `generate.bat` and the generated Visual Studio solution instead.

### Missing third-party code

If directories such as `deps/imgui` or `deps/minhook` are empty, run:

```powershell
git submodule update --init --recursive
```

### Wrong architecture

`Need for Speed: Underground 2` uses the 32-bit plugin.
Build and deploy `Release | Win-x86` unless you are targeting a different loader or a separate 64-bit environment.

## Branding note

ECM-R is a fork of the original ECM project by **BttrDrgn**.
The repository branding has been updated to ECM-R, and the runtime filenames follow the `ecm-r.*` naming scheme.
BASS is a third-party runtime dependency and remains subject to the official BASS license terms.
