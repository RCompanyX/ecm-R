<h1 align="center">ECM-R</h1>
<p align="center"><b>Fork Notice:</b> ECM-R is a fork of the original ECM (External Custom Music) project by BttrDrgn. Keep the original license notice and review the upstream project for attribution, licensing, and development history.</p>
<h2 align="center">External Custom Music Reloaded</h2>
<p align="center">A mod for Need for Speed: Underground 2 that plays custom music without overwriting the game's original files.</p>

## Overview

ECM-R replaces or mutes the in-game music and plays audio files from a user playlist folder.
It also includes an in-game overlay for playback control and runtime hotkey rebinding.

This fork is currently focused on **Need for Speed: Underground 2 (NFSU2)**.

The maintained runtime for this fork is currently the **Win-x86** build, deployed as `ecm-r.x86.asi` with `ecm-r.x86.ini`, `bass.dll`, and the `ecm-r/translations` folder beside it.

## Quick Start

1. Download the official Windows 32-bit BASS ZIP package from the `BASS` page on https://www.un4seen.com/.
2. Copy `bass.dll` from the root of the downloaded ZIP.
3. Extract ECM-R runtime files into the game's `scripts` folder:
   - `ecm-r.x86.asi`
   - `ecm-r.x86.ini` (ECM-R creates it automatically on first launch)
   - `bass.dll`
   - `ecm-r/translations/en.ini` and `ecm-r/translations/es.ini`
4. Create a `Music` folder next to those runtime files and place your supported audio files inside.
5. Launch the game, wait for the first startup music banner to finish, and then open the overlay with `F11`.

ECM-R supports separate `frontend_volume` and `ingame_volume` settings. Legacy `volume` configurations automatically use it as fallback when context-specific values are missing.

## Quick Links

- [Building](BUILDING.md)
- [Configuration Manual](CONFIGURATION.MD)
- [Changelog](CHANGELOG.md)

## Fork Status

This repository is maintained as a fork of the original ECM project by **BttrDrgn**. The current feature set and documentation are maintained around the NFSU2 runtime used by this fork.

For the original ECM source, attribution chain, or upstream history, review the upstream repository and the included license file.

## Credits and Support

- **Original ECM project:** BttrDrgn
- **NFSU2 reverse-engineering reference:** [yugecin/nfsu2-re](https://github.com/yugecin/nfsu2-re) for publishing reverse-engineering notes and documentation used as reference material during ECM-R development.
- **Current ECM-R fork maintainer:** RCompanyX
- **Bug reports, feature requests, and ideas:** [GitHub Issues](https://github.com/RCompanyX/ecm-R/issues)

The in-game overlay also includes an **About** menu with the same fork attribution and support links.

## Requirements

- Windows
- Need for Speed: Underground 2
- A working ASI loading setup or a compatible mod manager
- Microsoft Visual C++ Redistributable (x86): https://aka.ms/vs/17/release/vc_redist.x86.exe

## Building

Build instructions are in [BUILDING.md](BUILDING.md). Pre-built CI artifacts are also available — see the [CI Builds section](BUILDING.md#ci-builds-github-actions) for details.
Direct pushes to `main` publish `ECM-R-Win32.zip` with the ASI, translations, `README.md`, and `LICENSE`; other CI builds remain `ecm-r-build`.
A manually triggered `Debug | Win-x86` workflow also provides a diagnostic artifact containing the DLL, ASI, and PDB.

## Runtime Layout

Typical deployment for this fork places the runtime files together inside the game's `scripts` folder:

```text
Game Folder/
  scripts/
    ecm-r.x86.asi
    ecm-r.x86.ini
    bass.dll
    ecm-r/
      ecm-r.x86.log
      ecm-r.x86.log.1  (optional backup)
      translations/
        en.ini
        es.ini
    Music/
      Artist - Song 01.mp3
      Artist - Song 02.ogg
```

ECM-R loads the official BASS runtime dynamically, so `bass.dll` must be present next to the ECM-R runtime files.
Download the official Windows 32-bit BASS ZIP package from the `BASS` page on https://www.un4seen.com/ and copy `bass.dll` from the root of that ZIP.

## Installation

Before installing, ensure you have:

1. Downloaded the official Windows 32-bit BASS ZIP package from the `BASS` page on https://www.un4seen.com/ and copied `bass.dll` from the root of that ZIP
2. Obtained the ECM-R runtime files:
   - `ecm-r.x86.asi`
   - `ecm-r.x86.ini` (created automatically on first launch if missing)
   - `bass.dll`
   - `ecm-r/translations/en.ini` and `ecm-r/translations/es.ini`
3. Created a `Music` folder in the target location with your supported audio files

### Installation Methods

**Mr. Modman** or **ASI Loader**: Extract the release `scripts` contents into your game's directory. Ensure the final deployed `scripts` folder contains `ecm-r.x86.asi`, `bass.dll`, `ecm-r/translations/en.ini`, `ecm-r/translations/es.ini`, and your `Music` folder. ECM-R will create `ecm-r.x86.ini` automatically if it's missing.

## Supported Audio Formats

ECM-R currently scans the playlist folder for these file types:

- `.wav`
- `.mp1` (legacy MPEG-1 Layer I; retained for compatibility)
- `.mp2`
- `.mp3`
- `.ogg` (core BASS Ogg Vorbis; Ogg Opus and Ogg FLAC require separate add-ons and are not supported by the current runtime)
- `.aif`

MP1 is a legacy compatibility format and is not a primary QA target. Prefer MP2, MP3, WAV, OGG Vorbis, or AIFF for new music and testing.

## Implemented Functionality

### Playback

- Loads custom music from a configurable playlist folder without replacing the original game files
- Supports shuffle and repeat playback modes with persistent configuration
- Supports manual pause and resume from both a hotkey and the overlay
- Supports previous and next track navigation from both hotkeys and the overlay
- Supports separate frontend and in-game volume levels, with legacy `volume` compatibility for older configurations
- Plays valid tracks in discovered order when shuffle is disabled
- Can stop playback after the last valid track when repeat is disabled
- Supports per-track routing for frontend-only, in-game-only, or shared playback

### Overlay and Controls

- Displays an in-game overlay with playback controls, a dedicated Hotkeys menu, and playlist browsing
- Allows runtime toggles for shuffle and repeat directly from the overlay and supports optional hotkeys for both actions
- Includes Pause/Resume, Previous, Skip, and overlay toggle actions that can be rebound at runtime from the overlay
- Rejects duplicate hotkey assignments and suspends ECM-R hotkey execution while the overlay is capturing a new binding
- ECM-R hotkeys stay locked only during the first startup music banner and become available after that banner has fully disappeared
- Uses real playback history for previous-track navigation while shuffle is enabled
- Omits files BASS cannot open from runtime playback and the read-only Playlist menu for the session while retaining them on disk and in `[trax]` for next-launch re-evaluation
- Bounds failed playback attempts to one playlist pass so unavailable files do not enter shuffle history or cause endless autoplay retries
- Parses filenames robustly, splitting on the first `-` character and trimming whitespace, for clean `Artist - Title` display in the chyron, overlay menu bar, and playlist menu
- Reads embedded metadata tags (ID3v1, ID3v2, Vorbis Comments, RIFF INFO) when available; `.ogg` metadata refers to core BASS Ogg Vorbis support, while Ogg Opus/FLAC add-ons are outside the current runtime; when tags are missing or unparseable, ECM-R falls back to the filename "Artist - Title" convention on a per-field basis (originally proposed and prototyped by [@DeathWrench](https://github.com/DeathWrench))
- Truncates long embedded artist and title values for the in-game chyron without splitting UTF-8 characters
- Includes an About menu with repository and issue tracker links, plus a red startup notice that says whether the newer GitHub build is a stable release or a testing pre-release
- Localizes the Actions, Hotkeys, Playlist, About, Language, status, feedback, and release-notice overlay text in English and neutral Spanish, with runtime switching

### Configuration and Persistence

- Creates `ecm-r.x86.ini` automatically on first launch
- Writes persistent `ecm-r.x86.log` under `ecm-r/` alongside translations, capturing bootstrap diagnostics before applying `[config] log_level` (`error`, `warning`, `info`, or `debug`); the active file is capped at 2 MiB with one rotating `.1` backup
- Saves runtime changes for shuffle, repeat, in-game movie muting, volume, and hotkey settings back to the configuration file
- Migrates older configurations by using legacy `volume` as the fallback source for context-specific volume settings
- Persists and repairs `[config] language = en` or `es`, loading editable UTF-8 bundles from `ecm-r/translations/en.ini` and `ecm-r/translations/es.ini`
- Migrates obsolete movie-muting entries to `[config] ingame_movie_muting = true`; an existing canonical value remains unchanged
- Supports configurable playlist location, key bindings, loading-screen music handling, and per-track routing rules
- Automatically adds newly discovered music files to `[trax]` with `ALL` routing and removes orphaned entries for deleted files on each startup

### Game Integration

- Can stop custom music during loading screens and resume normal playback flow afterward through the `stop_music_on_loading_screens` setting, which defaults to `true`
- Allows custom music to continue through loading screens when `stop_music_on_loading_screens` is set to `false`
- Includes validated muting for comic-style in-game movie sequences from the `Actions` overlay menu or the `[config]` `ingame_movie_muting` setting, enabled by default in fresh configurations
- Keeps the original game files untouched while replacing or muting game music through the mod runtime

## Configuration

The full configuration reference is available in [CONFIGURATION.MD](CONFIGURATION.MD).

Key supported settings include `frontend_volume`, `ingame_volume`, `shuffle_enabled`, `repeat_enabled`, `stop_music_on_loading_screens`, `ingame_movie_muting`, `language`, `log_level`, `toggle_overlay`, `pause_track`, `previous_track`, `skip_track`, `toggle_shuffle`, and `toggle_repeat`.

Use `None` or `Unbound` in the `[keys]` section to disable a binding manually. The new `toggle_shuffle` and `toggle_repeat` hotkeys default to `None` until you assign them.

## Roadmap

The current roadmap includes:

- **Multiple Playlists** - Switch between different music folders dynamically within the game
- **Advanced Context Filters** - More granular playback rules beyond FE/IG (events, game modes, etc.)
- **Volume Normalization** - Automatic level equalization across all tracks

## Notes

- `bass.dll` must be the official BASS runtime placed next to the ECM-R runtime files. Download the Windows 32-bit ZIP package from the `BASS` page on https://www.un4seen.com/ and copy the `bass.dll` that is stored in the root of the ZIP. The current runtime integration has been tested with BASS `v2.4.18.11`. Avoid unofficial, modified, or repackaged builds.
- If `bass.dll` is missing, incorrect version, or cannot be loaded, audio playback will fail. ECM-R shows Windows error text, numeric BASS diagnostics, and the exact path it tried in the startup popup.
- BASS is a third-party library subject to the official BASS license terms. ECM-R does not bundle or redistribute `bass.dll`; users must obtain the official runtime themselves.
- If your usage becomes commercial, review the official BASS licensing terms and obtain any required license before distribution. ECM-R is maintained as a non-commercial fork project.
- ECM-R ignores its hotkeys during the first startup chyron so the initial banner can complete cleanly before manual controls are used.
- The mod writes a crash dump file on unhandled exceptions.
- Diagnostic logs are stored under the module-relative `ecm-r/` folder as `ecm-r.x86.log`; rotation keeps at most `ecm-r.x86.log.1`, never creates `.2`, and never removes or rotates timestamped `.dmp` crash evidence. The logger creates `ecm-r/` when missing and does not migrate older root-level logs. Logs can contain module-relative paths, playlist filenames, and Windows/BASS error details; review them before sharing.
- Release builds retain the exact matching PDB as a separate build/symbol artifact for dump analysis; it is not part of the runtime package.
- This repository includes third-party dependencies and keeps the original MIT license notice.
- The fork branding is ECM-R, and the runtime filenames follow the `ecm-r.*` naming scheme.

## License

This project includes an MIT license file that retains the original copyright notice.
If you redistribute this fork, keep the existing license and attribution intact.
