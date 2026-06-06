# ECM-R Application Context

## Purpose

This document consolidates the functional and technical context of ECM-R so contributors and GitHub Copilot can reason about the repository from a shared source of truth.

It complements these existing documents:

- `README.md` for product overview and user-facing behavior.
- `BUILDING.md` for build and packaging.
- `CONFIGURATION.MD` for INI behavior.
- `.github/copilot-instructions.md` for repository-specific agent rules.

When this document and the code disagree, the code is authoritative. If that happens, update this document and the Copilot instructions in the same change.

## Copilot Entry Point

GitHub Copilot reads the repository instruction file at `.github/copilot-instructions.md`.

That instruction file should point the agent here before non-trivial ECM-R work so the agent has a repository-level model of:

- the NFSU2 game-state model,
- the audio pause/resume flow,
- the hook surfaces that control playback,
- the overlay and hotkey model,
- the configuration and persistence rules,
- the runtime dependency boundary around `bass.dll`.

## Product Summary

ECM-R, short for External Custom Music Reloaded, is a Windows ASI-style mod focused on Need for Speed: Underground 2.

The plugin replaces the gameplay music experience without modifying the original game assets. It does that by muting the game music paths that matter to ECM-R, loading user music from a folder next to the runtime files, and exposing an in-game overlay plus hotkeys for playback control.

The current repository is NFSU2-first and the active runtime path is effectively the 32-bit NFSU2 integration.

Important scope facts:

- The runtime is currently hardwired to `game_t::NFSU2` during attach.
- The old multi-game detection path remains commented out and is not the active behavior.
- The generated runtime names are compatibility-sensitive and must stay stable unless a task explicitly changes them.

## Runtime and Deployment Model

Typical deployment places the runtime files together inside the game's `scripts` folder:

```text
Game Folder/
  scripts/
    ecm-r.x86.asi
    ecm-r.x86.ini
    bass.dll
    Music/
      Artist - Song 01.mp3
      Artist - Song 02.ogg
```

Key deployment constraints:

- `ecm-r.x86.asi` is the loader-facing runtime artifact.
- `ecm-r.x86.ini` is created automatically if missing.
- `bass.dll` is not bundled by the repository and must be obtained from the official BASS distribution.
- ECM-R loads `bass.dll` dynamically from the same directory as the plugin module.
- The active build target for NFSU2 is `Release | Win-x86`.
- The runtime-facing startup guidance and the maintained documentation both assume deployment next to `ecm-r.x86.asi`.

## High-Level Architecture

| Area | Responsibility | Primary files |
| --- | --- | --- |
| Bootstrap and injection | DLL attach, thread bootstrap, MinHook setup, renderer backend initialization, NFSU2 memory patches | `src/app/main.cpp`, `src/app/global.*`, `src/app/hook/impl/*` |
| Game-state model | Shared interpretation of the current NFSU2 flow state | `src/app/defs.hpp` |
| Hook helpers and chyron control | FNG package checks, safe chyron summon/hide rules, helper patch primitives | `src/app/hook/hook.hpp` |
| Audio engine | Playlist discovery, context filtering, shuffle/repeat/history, pause and resume, BASS integration | `src/app/audio/audio.*`, `src/app/audio/player.*`, `src/app/audio/bass_api.*` |
| Input and hotkeys | Overlay toggle, playback hotkeys, rebinding capture, duplicate prevention, key polling | `src/app/input/input.*` |
| Overlay UI | ImGui menus, runtime controls, hotkeys UI, playlist listing, about dialog, release notice | `src/app/menus/menus.*` |
| Settings and persistence | INI creation, migration, runtime saves, default hotkeys, `[trax]` normalization | `src/app/settings/settings.*` |
| Build and packaging | Premake workspace, output naming, generated solution layout | `lua/windows.lua`, `generate.bat`, `BUILDING.md` |

## Runtime Lifecycle

### 1. DLL attach and worker thread

`DllMain` handles `DLL_PROCESS_ATTACH`, stores the module handle, disables per-thread notifications, and creates a worker thread.

The attach thread then:

1. Allocates a console.
2. Sets the console title to `ECM-R Debug Console`.
3. Redirects standard input and output to that console.
4. Hides the console in non-debug builds.
5. Forces `global::game = game_t::NFSU2`.
6. Calls `init()`.

### 2. Early runtime initialization in `init()`

`init()` performs the repository's critical low-level setup:

- Initializes MinHook.
- Applies the NFSU2-specific patches and hooks.
- Loads the persisted settings before audio starts.
- Initializes Kiero and selects the render backend.
- Installs backend-specific ImGui hooks.
- Enables all MinHook hooks.

### 3. First renderer frame initializes the live overlay stack

Audio and overlay initialization do not complete inside `main.cpp` alone.

Instead, the selected renderer backend performs the final runtime setup on the first hooked frame. In the D3D9 path this happens in `hkEndScene`, which:

1. Saves the game window handle to `global::hwnd`.
2. Calls `audio::init()`.
3. Calls `input::init_overlay()`.
4. Calls `menus::init()`.
5. Initializes the Win32 and renderer-specific ImGui backends.

This means ECM-R depends on a successful graphics hook to bring up both audio playback and the overlay.

### 4. Per-frame and per-tick updates

Two update loops matter:

- The NFSU2 main-loop hook calls `audio::update()` every game tick.
- The renderer hook calls `input::update()`, `menus::prepare()`, `menus::update()`, and `menus::present()` every hooked render frame.

## NFSU2 Game-State Model

The repository treats `src/app/defs.hpp` as the source of truth for game flow state interpretation.

Defined states:

- `None`
- `LoadingFrontend`
- `UnloadingFrontend`
- `InFrontend`
- `LoadingRegion`
- `LoadingTrack`
- `Racing`
- `UnloadingTrack`
- `UnloadingRegion`
- `ExitDemoDisc`

### Playlist context mapping

The audio layer derives playlist context from the game state:

| Game state | Effective playlist context |
| --- | --- |
| `LoadingFrontend`, `InFrontend` | Frontend |
| `UnloadingFrontend`, `LoadingRegion`, `LoadingTrack`, `Racing` | In-game |
| Any other state | All |

This mapping matters because `[trax]` routing values are interpreted against the derived context.

### Loading-state behavior

The audio layer treats these states as loading states for the `stop_music_on_loading_screens` behavior:

- `LoadingFrontend`
- `LoadingRegion`
- `LoadingTrack`

If loading-screen stopping is enabled, ECM-R stops the current custom track instead of simply pausing it.

### Chyron safety rules

The repository also treats state as part of UI safety for the in-game notification banner.

`hook::SummonChyron()` refuses to show a chyron in these states:

- `None`
- `LoadingFrontend`
- `LoadingRegion`
- `LoadingTrack`
- `ExitDemoDisc`

In frontend-related states, the chyron also waits for frontend UI packages to be available.

## Audio and Playback Model

### Core goal

The audio subsystem replaces the audible gameplay music path with external files while keeping playlist behavior coherent across frontend, loading, and racing states.

### BASS loading model

The repository has been migrated to manual dynamic loading of BASS.

`bass_api::load()`:

- derives the plugin directory from the loaded ECM-R module,
- builds the expected path to `bass.dll`,
- calls `LoadLibraryA`,
- resolves the required BASS exports with `GetProcAddress`,
- stores detailed Windows error text if loading fails.

If `bass.dll` is missing, wrong, or incomplete:

- ECM-R shows a startup popup,
- logs the error,
- sets `global::shutdown = true`,
- disables music for the session.

The current code validates the BASS version against the expected `0x204` major version family.

### Playlist discovery

Settings initialization resolves the playlist directory from the plugin path and the configured playlist folder name.

`audio::enumerate_playlist()` scans the folder for supported file types and records each discovered track as:

- full path,
- routing context from `[trax]`, normalized to `ALL`, `FE`, or `IG`.

Supported file extensions currently documented in the repository are:

- `.wav`
- `.mp1`
- `.mp2`
- `.mp3`
- `.ogg`
- `.aif`

### Track context filtering

Each track may be tagged as:

- `ALL`: valid everywhere,
- `FE`: frontend only,
- `IG`: in-game only.

Normalization rules in settings loading are strict:

- quoted and whitespace-padded values are trimmed,
- values are uppercased,
- invalid or missing values fall back to `ALL`.

At runtime:

- `ALL` and legacy `N/A` are treated as universally valid,
- frontend context rejects `IG`,
- in-game context rejects `FE`.

### Playlist order, shuffle, repeat, and history

The current playback system is centered on a generated `playlist_order` rather than raw file order.

Important behavior:

- The order is rebuilt when the effective context changes.
- The order is rebuilt when shuffle is toggled.
- Non-shuffle mode clears playback history.
- Shuffle mode keeps a bounded playback history so `Previous` can walk real history instead of fabricating reverse order.
- Repeat disabled allows the playlist to end after the last valid track.
- Repeat enabled rebuilds the order and wraps back to the start.

The repository prefers the shared helper approach for navigation:

- `play_next_song()`
- `play_previous_song()`

Both route through shared relative-playback logic instead of branching on a direction flag.

### Pause model

ECM-R distinguishes between two pause causes:

- `manual_paused`: the user paused playback through the overlay or a hotkey.
- `game_paused`: the game flow or package hooks require playback to pause.

The effective `paused` state is the union of those flags.

Consequences:

- Pausing hides the chyron completely.
- Resuming can request the current chyron again.
- Resuming only continues the current song if that song is still valid for the current playlist context.
- If the current song is no longer valid for the context, the system stops it and moves to the next valid track.

### Startup and automatic playback flow

`audio::init()` performs these steps:

1. Registers the NFSU2 mute-detection package list.
2. Loads and validates `bass.dll`.
3. Initializes the BASS device using the captured game window handle.
4. Builds the current playlist order.
5. Sets the audio layer into the paused state.
6. Runs an initial `audio::update()`.

This means playback does not immediately start just because BASS is available. Normal playback depends on later resume conditions from the game integration.

### Loading screens and movie-related muting

The current NFSU2 mute-detection list includes these packages:

- `LS_PSAMovie.fng`
- `LS_THXMovie.fng`
- `LS_EAlogo.fng`
- `LS_BlankMovie.fng`
- `UG_LS_IntroFMV.fng`

These are treated as game-owned sequences where ECM-R should pause its custom playback.

The NFSU2 reverse-engineering references document `IG_PlayMovie.fng` as the in-game movie package used by career movie playback paths such as `PostRaceFNGObject::PlayMovieIfNeeded`.

ECM-R keeps that package behind the `[experimental]` `ingame_movie_muting` flag. When the flag is `true`, `IG_PlayMovie.fng` is added to `audio::mute_detection` and the runtime uses the experimental package-reconciliation path. When the flag is `false`, ECM-R keeps the legacy package-hook behavior and does not treat `IG_PlayMovie.fng` as a mute trigger.

### Metadata and chyron text

Playback metadata is derived primarily from filenames.

If a filename matches `Artist - Title.ext`, ECM-R uses:

- the left side as artist,
- the right side as title,
- the playlist name as the album or source label.

If the filename does not match that pattern cleanly, ECM-R falls back to simpler placeholders.

## Hook Map and Low-Level Integration

The current NFSU2 integration depends on several direct patches and hooks in `src/app/main.cpp`.

| Address or hook point | Purpose |
| --- | --- |
| `0x0083AA30` and `0x0083AA34` | Force FE and IG game music volume to `0.0f` during `sys_init_()` |
| `0x00534535` | Prevent save data from reloading game audio values that conflict with ECM-R |
| `0x004B6EDA -> 0x004B6F92` | Disable the frontend audio sliders |
| `0x004C347B -> 0x004C3533` | Disable the in-game pause-menu audio sliders |
| `0x0057EDA3 -> sys_init` | Insert ECM-R system init hook |
| `0x005811E4 -> NFSU2_MainLoop` | Run `audio::update()` every NFSU2 main-loop tick |
| `0x00537980` via MinHook | Intercept package loads for ECM-R mute handling |

### Package-load hook behavior

The intercepted package-load function currently does this:

- With `[experimental] ingame_movie_muting = false`, ECM-R keeps the legacy behavior: pause when the loaded package matches the mute-detection list, otherwise resume if `audio::game_paused` is already set.
- With `[experimental] ingame_movie_muting = true`, ECM-R first calls the original package-load function and then reconciles pause state against the mute packages that are actually loaded.

When the experimental path is enabled, `audio::update()` also repeats this reconciliation each game tick so package transitions that do not cleanly map to a single `ShowFNG` event still keep ECM-R paused for the full lifetime of the active movie package.

## Overlay, Input, and User Controls

### Overlay fundamentals

The overlay is hidden by default because `global::hide` starts as `true`.

The default way to open it is the `toggle_overlay` hotkey, which defaults to `F11`.

The overlay currently provides:

- an `Actions` menu for volume and playback control,
- an `Experimental` menu for runtime-only feature flags,
- a `Hotkeys` menu for runtime rebinding,
- a `Playlist` menu that lists discovered track names,
- an `About` menu with attribution and support links.

### Actions menu behavior

The `Actions` menu exposes:

- frontend and in-game volume controls,
- pause and resume,
- previous track,
- skip track,
- shuffle toggle,
- repeat toggle,
- current context, active volume, and track count display.

The volume UI intentionally reflects the active context so frontend and in-game values can be tuned separately.

### Hotkey model

Supported actions are:

- `toggle_overlay`
- `pause_track`
- `previous_track`
- `skip_track`
- `toggle_shuffle`
- `toggle_repeat`

Default bindings:

- `toggle_overlay = F11`
- `pause_track = F8`
- `previous_track = F9`
- `skip_track = F10`
- `toggle_shuffle = None`
- `toggle_repeat = None`

Important hotkey rules:

- Duplicate assignments are rejected.
- Unsupported keys are rejected.
- The overlay does not expose a `Clear` button for `toggle_overlay` to reduce the chance of losing access to the UI.
- While capture mode is active, ECM-R suspends hotkey execution so the candidate key does not trigger playback or overlay actions.
- Some actions are handled both from the window procedure and from `GetAsyncKeyState` polling to remain responsive in the hooked environment.

### First-chyron lockout

ECM-R deliberately keeps its hotkeys locked until the first startup chyron has been seen loaded and then unloaded once.

This is an important stability rule because interrupting the first banner too early can break later chyron behavior for the session.

### Playlist menu limitations

The current `Playlist` menu is read-only. It lists discovered song names but does not edit track routing or metadata at runtime.

### About menu and release notice

The overlay includes:

- repository and issue-tracker links,
- fork attribution to the original ECM project and BttrDrgn,
- a startup version notice if GitHub reports a newer build.

Version discovery currently:

- dynamically loads `winhttp.dll`,
- queries the GitHub releases list endpoint,
- parses release tags manually,
- uses the `latest_non_draft` policy instead of the `/latest` endpoint.

That policy exists because public releases in this repository may be marked as pre-release, which makes `/releases/latest` unsuitable.

## Settings and Persistence

### Configuration file identity

The active configuration filename is hardcoded to `ecm-r.x86.ini`.

The settings subsystem resolves it relative to the plugin location and manages the file automatically.

### Configuration sections

The generated configuration contains these sections:

- `[core]`
- `[config]`
- `[experimental]`
- `[keys]`
- `[trax]`

### Persisted behavior

The settings layer persists:

- playlist folder name,
- legacy and context-specific volume values,
- shuffle and repeat flags,
- loading-screen handling,
- hotkey bindings,
- per-track routing in `[trax]`.

### Migration and repair behavior

The settings layer also repairs or migrates older configurations by:

- using legacy `volume` as the fallback source for `frontend_volume` and `ingame_volume`,
- adding missing config keys,
- restoring invalid or duplicate hotkey entries to safe defaults,
- rewriting the config when version or structure drift is detected.

### Track auto-population in [trax]

When the settings layer loads an existing INI and discovers music files that are not yet listed in `[trax]`, those tracks are playable in memory (assigned `ALL` routing) and are also automatically written to the INI via `settings::sync_trax_entries()`. This ensures newly added music files persist to the configuration without requiring a version change or a full config rewrite.

This makes settings work an area where behavior, migration, and persistence must stay aligned.

## Build and Packaging Context

The plugin is not built from a top-level CMake configuration.

The authoritative build flow is:

1. Run `generate.bat`.
2. Open `build/ECM-R.sln`.
3. Build `Release | Win-x86`.

The Premake workspace in `lua/windows.lua` defines the output naming and the post-build `.asi` copy step.

Although the workspace exposes both `Win-x86` and `Win-x64` platforms, the current repository context is still x86-first:

- the documented validation target is `Release | Win-x86`,
- the active deployment filenames are `ecm-r.x86.asi` and `ecm-r.x86.ini`,
- `settings::config_file` is still hardcoded to `ecm-r.x86.ini`,
- the code comments still describe x64 as not yet ready for normal use.

Expected output paths include:

- `build/bin/Release-Win-x86/x86/ecm-r.x86.dll`
- `build/bin/Release-Win-x86/x86/ecm-r.x86.asi`

## Known Boundaries and Non-Obvious Constraints

- ECM-R currently assumes NFSU2 first; do not describe the repository as broadly multi-game unless the runtime behavior changes.
- Audio startup depends on renderer-hook success because `audio::init()` runs from the render backend.
- Chyron behavior is tightly coupled to both game state and FNG package availability.
- The loading-screen option stops custom audio entirely instead of keeping a resumable pause token for that track.
- Track routing is currently coarse-grained: `ALL`, `FE`, and `IG` only.
- The overlay lists tracks but does not manage playlist metadata or `[trax]` assignments directly.
- The `Experimental` overlay menu currently exposes only `ingame_movie_muting`, and that flag is also persisted in the `[experimental]` INI section.
- Runtime filenames and deployment expectations are compatibility-sensitive.

## Source-of-Truth File Map for Future Changes

Use this file map when scoping work:

- Game-state meaning: `src/app/defs.hpp`
- Attach flow and NFSU2 patch sites: `src/app/main.cpp`
- Chyron gating and package checks: `src/app/hook/hook.hpp`
- Audio behavior and playlist flow: `src/app/audio/audio.cpp`
- BASS dynamic loading boundary: `src/app/audio/bass_api.cpp`
- Playback metadata extraction: `src/app/audio/player.cpp`
- Hotkey behavior and rebinding safety: `src/app/input/input.cpp`
- Overlay behavior and release notice logic: `src/app/menus/menus.cpp`
- INI creation, migration, and persistence: `src/app/settings/settings.cpp`
- Build outputs and naming: `lua/windows.lua`
- User-facing product description: `README.md`
- Configuration contract: `CONFIGURATION.MD`
- Build contract: `BUILDING.md`

## Reliability Checklist for Contributors and Copilot

Before changing playback, hooks, or settings, verify all of the following:

- Which `GameFlowState` values are affected.
- Whether the change alters playlist context selection.
- Whether the change alters loading-screen stop behavior.
- Whether the change changes when `audio::pause()` or `audio::play()` is reached.
- Whether chyron visibility remains correct when pausing and resuming.
- Whether the overlay and hotkeys still call the same shared helpers.
- Whether any hotkey rebinding edge case can cause live actions during capture.
- Whether configuration persistence and migration still match the runtime behavior.
- Whether the runtime filenames, `bass.dll` expectations, and deployment layout remain stable.

For audio-related work, the most failure-prone regression areas are:

- frontend to in-game transitions,
- loading-screen behavior,
- first-chyron startup lock behavior,
- pause and resume coherence,
- shuffle history correctness,
- persistence of new runtime toggles.
