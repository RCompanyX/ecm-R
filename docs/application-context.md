# ECM-R Application Context

## Purpose

This document consolidates the functional and technical context of ECM-R so contributors and AI coding agents (OpenCode, GitHub Copilot) can reason about the repository from a shared source of truth.

It complements these existing documents:

- `README.md` for product overview and user-facing behavior.
- `BUILDING.md` for build and packaging.
- `CONFIGURATION.MD` for INI behavior.
- `AGENTS.md` for repository-specific agent rules and workflows.

When this document and the code disagree, the code is authoritative. If that happens, update this document and `AGENTS.md` in the same change.

`AGENTS.md` owns imperative operational rules; this document owns stable architecture and runtime behavior.

## Agent Entry Point

Agents should read `AGENTS.md` first for workflow and rule instructions, then this document before any non-trivial ECM-R work to build a repository-level model of:

- the NFSU2 game-state model,
- the audio pause/resume flow,
- the hook surfaces that control playback,
- the overlay and hotkey model,
- the configuration and persistence rules,
- the runtime dependency boundary around `bass.dll`.

### Agent Architecture

ECM-R defines four specialized subagents for development workflows. They are defined in `.opencode/agents/` and managed through OpenCode.

| Agent | Role | Permissions | Key constraint |
| --- | --- | --- | --- |
| `ecmr-explore` | Feasibility | Read-only | Entry point; never edits files or runs builds; delegates viable ideas to `ecmr-plan` |
| `ecmr-plan` | Planning | Read-only | Runs after feasibility assessment; never edits files; delegates execution to `ecmr-dev` |
| `ecmr-dev` | Development | Read-write | Works from the active branch under `AGENTS.md` branch policy; builds `Release\|Win-x86` |
| `ecmr-release` | Releases | Read-write | Manages version bumps, changelog, and release notes; hard-skip boundaries are defined in `AGENTS.md` |

#### Idea explorer (`ecmr-explore`)

`ecmr-explore` is the entry point for raw feature, bug, enhancement, or question requests. It researches the codebase, assesses feasibility and impact, and classifies each request as `VIABLE`, `NOT_VIABLE`, or `NEEDS_CLARIFICATION`. It is read-only, never edits files or runs builds, and delegates viable requests to `ecmr-plan` with the full feasibility assessment.

The detailed explorer output template is defined in `.opencode/agents/ecmr-explore.md`.

#### Planning agent (`ecmr-plan`)

After `ecmr-explore` classifies a request as `VIABLE`, `ecmr-plan` assesses implementation risk and produces the implementation plan. Plans cover: affected `GameFlowState` values, audio transitions, hook surfaces, overlay flows, and persisted settings (per AGENTS.md §8).

CHANGELOG entries belong under `## [Unreleased]`. Plans must compare each entry against the base/target branch, normally `main`, and include the classification rationale: a feature absent from the base is `### Added`; behavioral changes to that same in-progress feature are `### Changed`; `### Fixed` is reserved for regressions of existing base-branch behavior or clearly marked bugs introduced and fixed during the current work.

After the plan is approved, `ecmr-plan` delegates execution to `ecmr-dev` via the Task tool with the full plan text.

#### Developer agent (`ecmr-dev`)

Receives approved plans and implements them. Responsibilities:

- Works on the active working branch; creates a `dev_...` branch only when starting from `main` or when explicitly requested (per AGENTS.md §10 branch policy).
- Implements features, bug fixes, or documentation changes.
- Builds the plugin targeting `Release | Win-x86`.
- Keeps `CHANGELOG.md` `## [Unreleased]` entries updated as work progresses, classifying each entry against the base/target branch.
- Commit, push, and GitHub write approval rules are defined in `AGENTS.md` §10.
- Follows all rules in AGENTS.md and `docs/application-context.md`.

#### Release agent (`ecmr-release`)

Prepares versioned releases. Workflow:

- Bumps semantic version in `src/app/stdafx.hpp`.
- Renames `## [Unreleased]` to the new version tag in CHANGELOG.md.
- Generates release notes in `docs/releases/vX.Y.Z-alpha.md`.
- Syncs user-facing documentation: `README.md`, `CONFIGURATION.MD`, `BUILDING.md`.

The release agent's hard-skip boundaries are defined in `AGENTS.md` §11; this document records its release workflow without duplicating those restrictions.

#### Typical workflow

`ecmr-explore` (assess) → `ecmr-plan` (plan) → `ecmr-dev` (implement) → `ecmr-release` (release).

See AGENTS.md for detailed agent instructions and rules.

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

Key deployment constraints:

- `ecm-r.x86.asi` is the loader-facing runtime artifact.
- `ecm-r.x86.ini` is created automatically if missing.
- `ecm-r/ecm-r.x86.log` is created alongside the translation bundles; its active size is capped at 2 MiB and rotation retains at most `ecm-r/ecm-r.x86.log.1`. The logger creates `ecm-r/` when missing, validates an existing path as a directory, and does not migrate older root-level logs.
- `bass.dll` is not bundled by the repository and must be obtained from the official BASS distribution. ECM-R has no static BASS SDK or SDK-header dependency; no BASS binaries are committed, and third-party redistributions are not accepted.
- ECM-R loads `bass.dll` dynamically from the same directory as the plugin module.
- The active build target for NFSU2 is `Release | Win-x86`.
- Editable overlay translations are deployed as UTF-8 `ecm-r/translations/en.ini` and `ecm-r/translations/es.ini` beside the plugin.
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
| Localization | Startup-cached UTF-8 English/Spanish bundles, placeholder validation, runtime locale selection | `src/app/localization/localization.*`, `ecm-r/translations/*.ini` |
| Diagnostics | Persistent module-relative log, level filtering, bounded rotation, console fallback | `src/utils/logger/logger.hpp` |
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
6. Initializes the persistent logger at bootstrap `debug` level.
7. Calls `init()`.

Unexpected worker-thread failures are handled by `CustomUnhandledExceptionFilter`, which writes `ecm-r-YYYYMMDDHHMMSS.dmp` next to the plugin and uses `MessageBoxA` to report the path. The filter does not call the normal logger.

### Persistent diagnostics

`src/utils/logger/logger.hpp` keeps the public console logging API while adding a module-relative `ecm-r/ecm-r.x86.log` sink alongside the translation bundles. The logger starts before MinHook, memory patches, settings, and renderer selection, so bootstrap diagnostics use an unfiltered `debug` phase. After `[config]` is loaded, only `error`, `warning`, `info`, or `debug` entries at the persisted `log_level` are emitted; the default is `info`. Applying the level never closes, truncates, or recreates the active file.

The active log is limited to 2 MiB. Before a record would exceed the limit, the logger flushes and closes the active file, removes `.1`, renames the active file to `.1`, opens a new active file with a session header, and writes the triggering record. There is no `.2`, timestamped log, directory scan, or retry loop. Oversized individual records are bounded with an explicit truncation marker. File failures disable only the file sink for the session and leave console diagnostics and runtime operation intact.

Logs can contain module-relative paths, playlist filenames, and Windows/BASS error details; they should be reviewed before sharing. Older root-level logs are left untouched. `.dmp` files remain separate crash evidence beside the module and are never automatically cleaned or rotated. Release PDBs are generated and archived separately from user runtime packages.

### 2. Early runtime initialization in `init()`

`init()` performs the repository's critical low-level setup:

- Initializes MinHook.
- Applies the NFSU2-specific patches and hooks.
- Loads the persisted settings before audio starts.
- Loads and validates both translation bundles during settings initialization, before BASS startup dialogs can appear.
- Selects the render backend. When `d3d9.dll` is already loaded, ECM-R bypasses Kiero's synthetic D3D9 `NULLREF` device probe and arms a live `Direct3DCreate9`/`IDirect3D9::CreateDevice` capture hook instead. It also obtains a factory through the already-loaded export to recover the shared `CreateDevice` entry when the game's export callback was missed; it never creates a device or enumerates already-created devices.
- Installs backend-specific ImGui hooks. The live D3D9 device's `EndScene`, `Present`, and `Reset` methods are bound only after the game creates that device.
- Enables all MinHook hooks.

### 3. First renderer frame initializes the live overlay stack

Audio and overlay initialization do not complete inside `main.cpp` alone.

Instead, the selected renderer backend performs the final runtime setup on the first live callback. In the D3D9 path this happens in the game-created device's `hkEndScene` or `hkPresent`, which:

1. Saves the game window handle to `global::hwnd`.
2. Calls `audio::init()`.
3. Calls `input::init_overlay()`.
4. Calls `menus::init()`.
5. Initializes the Win32 and renderer-specific ImGui backends.

This means ECM-R depends on a successful graphics hook to bring up both audio playback and the overlay.
The D3D9 path records the first live callback and fails closed after a bounded no-callback timeout, reporting whether `Direct3DCreate9` and `CreateDevice` callbacks occurred; it keeps ECM-R loaded because direct game hooks may still point at the module. If the game device already existed before ECM-R loaded, the public D3D9 API provides no safe device enumeration path, so the watchdog remains the fail-closed boundary.

### 4. Per-frame and per-tick updates

Two update loops matter:

- The NFSU2 main-loop hook calls `audio::update()` every game tick.
- The renderer hook calls `input::update()`, `menus::prepare()`, `menus::update()`, and `menus::present()` every hooked render frame.

## NFSU2 Game-State Model

The repository treats `src/app/defs.hpp` as the source of truth for game flow state interpretation.
The live `GameFlowState` value is read from memory address `0x008654A4` through `game_state`.

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

When a notification is suppressed, ECM-R hides/removes the chyron rather than rendering it with empty text.

## Audio and Playback Model

### Core goal

The audio subsystem replaces the audible gameplay music path with external files while keeping playlist behavior coherent across frontend, loading, and racing states.

### BASS loading model

The repository uses a dynamic-only BASS boundary: `LoadLibraryA` loads `bass.dll` and `GetProcAddress` resolves its required exports. ECM-R has no static BASS SDK or SDK-header dependency, and no BASS binaries are committed.

`bass_api::load()`:

- derives the plugin directory from the loaded ECM-R module,
- builds the expected path to `bass.dll`,
- calls `LoadLibraryA`,
- resolves the required BASS exports with `GetProcAddress`,
- stores detailed Windows error text if loading fails.

If BASS loading fails, device initialization fails, or the BASS version mismatches:

- ECM-R shows a startup popup,
- static popup templates follow the selected overlay language,
- logs the error,
- sets `global::shutdown = true`,
- disables audio for the session.

The current code validates the BASS version against the expected `0x204` major version family.

When opening a stream for playback, ECM-R converts the UTF-8 file path to a wide string and passes it with the `BASS_UNICODE` flag so non-ASCII filenames are resolved correctly.

### Playlist discovery

Settings initialization resolves the playlist directory from the plugin path and the configured playlist folder name.

`audio::enumerate_playlist()` scans the folder for supported file types and records each discovered track as:

- full path (stored internally as UTF-8),
- routing context from `[trax]`, normalized to `ALL`, `FE`, or `IG`.

Directory enumeration uses wide-string `std::filesystem` APIs and converts paths back to UTF-8 for internal storage. Extension matching is case-insensitive so `.mp3`, `.MP3`, and `.Mp3` are all recognized correctly.

Supported file extensions currently documented in the repository are:

- `.wav`
- `.mp1` (legacy MPEG-1 Layer I; retained for compatibility)
- `.mp2`
- `.mp3`
- `.ogg` (core BASS Ogg Vorbis; Ogg Opus and Ogg FLAC require separate add-ons and are not supported by the current runtime)
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
- Files rejected by the startup BASS probe are excluded from runtime order, context counts, metadata resolution, and the read-only Playlist menu for the session.
- `playlist_files` remains unchanged so `[trax]` synchronization retains rejected files; enumeration clears the session cache so the next startup probe can re-evaluate them.

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
4. Probes each discovered file once with BASS, caching files that cannot be opened.
5. Builds the current playlist order, excluding cached-unplayable files.
6. Sets the audio layer into the paused state.
7. Runs an initial `audio::update()`.

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

ECM-R controls that package with the normal `[config]` `ingame_movie_muting` setting. When enabled, `IG_PlayMovie.fng` is added to `audio::mute_detection` and the runtime reconciles pause state against loaded packages. Fresh configurations enable it by default. When disabled, ECM-R keeps the legacy package-hook behavior and does not treat `IG_PlayMovie.fng` as a mute trigger.

### Metadata and chyron text

Playback metadata is resolved in this order:

1. **Embedded tags** (per-file-format):
   - MP3 / MP2 / AIF: ID3v2 (`TIT2` for title, `TPE1` for artist) first; ID3v1 second (per-field fallback, never overwrites ID3v2).
   - MP1: the same ID3 path is retained for legacy compatibility and is not a primary QA target.
   - OGG: Vorbis Comments (`TITLE`, `ARTIST`) for core BASS Ogg Vorbis streams; Ogg Opus/FLAC add-ons are not loaded by the current runtime.
   - WAV: RIFF INFO (`INAM`, `IART`).
2. **Filename fallback** (per-field — only fills fields still `"N/A"` after tag parsing):
   - If the filename matches `Artist - Title.ext`, ECM-R uses the left side as artist and the right side as title.
   - If the filename does not match that pattern, the whole filename (minus extension) becomes the title and artist stays `"N/A"`.
3. `playlist_name` is used as the album/source label (`where` field), independent of any embedded album tags.

Tags that are absent or malformed are silently skipped without crashing. Metadata resolution does not change playlist context, GameFlowState, or the playback state machine.

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
| `d3d9.dll!Direct3DCreate9` via MinHook | Capture the game's live D3D9 factory without creating a synthetic device |
| Live `IDirect3D9::CreateDevice` via MinHook | Capture the game-created D3D9 device |
| Live `IDirect3DDevice9::Reset`/`Present`/`EndScene` via MinHook | Rebuild ImGui resources and render the overlay |

### Package-load hook behavior

The intercepted package-load function currently does this:

- With `[config] ingame_movie_muting = false`, ECM-R keeps the legacy behavior: pause when the loaded package matches the mute-detection list, otherwise resume if `audio::game_paused` is already set.
- With `[config] ingame_movie_muting = true`, ECM-R first calls the original package-load function and then reconciles pause state against the mute packages that are actually loaded.

When movie muting is enabled, `audio::update()` also repeats this reconciliation each game tick so package transitions that do not cleanly map to a single `ShowFNG` event still keep ECM-R paused for the full lifetime of the active movie package.

## Overlay, Input, and User Controls

### Overlay fundamentals

The overlay is hidden by default because `global::hide` starts as `true`.

The default way to open it is the `toggle_overlay` hotkey, which defaults to `F11`.

The overlay currently provides:

- an `Actions` menu for volume and playback control,
- a `Hotkeys` menu for runtime rebinding,
- a `Playlist` menu that lists discovered track names,
- an `About` menu with attribution and support links,
- a `Language` submenu for English and neutral Spanish.

An empty `Experimental` menu entry point remains retained internally for possible future controls, but it is not invoked by the current UI.

### Actions menu behavior

The `Actions` menu exposes:

- frontend and in-game volume controls,
- pause and resume,
- previous track,
- skip track,
- shuffle toggle,
- repeat toggle,
- in-game movie muting toggle and state,
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
- Overlay labels and feedback use the active cached UTF-8 bundle; track metadata, filenames, paths, internal identifiers, INI keys, and game chyrons remain untranslated.
- Language changes are persisted immediately and applied after the current ImGui draw, so the following frame uses one consistent bundle. Active hotkey capture remains active while transient feedback is cleared.
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
- `[keys]`
- `[trax]`

`[config] language` accepts `en` or `es`, defaults to `en`, and invalid or missing values are repaired to `en`. `[config] log_level` accepts `error`, `warning`, `info`, or `debug`, defaults to `info` after bootstrap, and invalid or missing values are repaired to `info`.

### Persisted behavior

The settings layer persists:

- playlist folder name,
- legacy and context-specific volume values,
- shuffle and repeat flags,
- loading-screen handling,
- in-game movie muting,
- overlay language,
- diagnostic log level,
- hotkey bindings,
- per-track routing in `[trax]`.

### Migration and repair behavior

The settings layer also repairs or migrates older configurations by:

- using legacy `volume` as the fallback source for `frontend_volume` and `ingame_volume`,
- respecting `[config] ingame_movie_muting` when present; otherwise promoting old `[experimental] ingame_movie_muting` or legacy `[config] experimental_ingame_movie_muting` entries to `true`,
- defaulting missing movie-muting values to `true` and rewriting obsolete placements out of the INI,
- adding missing config keys,
- repairing missing or invalid `[config] language` values to `en`,
- repairing missing, invalid, or non-canonical `[config] log_level` values to `info`,
- restoring invalid or duplicate hotkey entries to safe defaults,
- rewriting the config when version or structure drift is detected.

### Track auto-population in [trax]

When the settings layer loads an existing INI and discovers music files that are not yet listed in `[trax]`, those tracks are playable in memory (assigned `ALL` routing) and are also automatically written to the INI via `settings::sync_trax_entries()`. This ensures newly added music files persist to the configuration without requiring a version change or a full config rewrite.

The same function also cleans up orphaned `[trax]` entries — keys whose corresponding files no longer exist on disk. This keeps the INI free of stale entries without manual maintenance.

This makes settings work an area where behavior, migration, and persistence must stay aligned.

## Build and Packaging Context

The plugin is not built from a top-level CMake configuration.

The authoritative build flow is:

1. Run `generate.bat`.
2. Open `build/ECM-R.sln`.
3. Build `Release | Win-x86`.

The exact command-line build is:

`msbuild build\ECM-R.sln /t:Overlay /p:Configuration=Release /p:Platform=Win-x86 /m`

The `VERSION` macro in `src/app/stdafx.hpp` is the semver source of truth and is bumped only by `ecmr-release`.

The Premake workspace in `lua/windows.lua` defines the output naming and the post-build `.asi` copy step.

Although the workspace exposes both `Win-x86` and `Win-x64` platforms, the current repository context is still x86-first:

- the documented validation target is `Release | Win-x86`,
- the active deployment filenames are `ecm-r.x86.asi` and `ecm-r.x86.ini`,
- `settings::config_file` is still hardcoded to `ecm-r.x86.ini`,
- the code comments still describe x64 as not yet ready for normal use.

Expected output paths include:

- `build/bin/Release-Win-x86/x86/ecm-r.x86.dll`
- `build/bin/Release-Win-x86/x86/ecm-r.x86.asi`
- `build/bin/Release-Win-x86/x86/ecm-r.x86.pdb` (archived separately from the runtime package for symbolization)
- `build/bin/Release-Win-x86/x86/ecm-r/translations/en.ini`
- `build/bin/Release-Win-x86/x86/ecm-r/translations/es.ini`

## Known Boundaries and Non-Obvious Constraints

- ECM-R currently assumes NFSU2 first; do not describe the repository as broadly multi-game unless the runtime behavior changes.
- Audio startup depends on a live renderer callback because `audio::init()` runs from the render backend. Game-loop and package callbacks remain inert until the BASS device is initialized.
- D3D9 renderer setup avoids Kiero's synthetic `NULLREF` probe and requires the game-created device. If no live callback arrives, a bounded watchdog disables audio without unloading the module.
- Chyron behavior is tightly coupled to both game state and FNG package availability.
- The loading-screen option stops custom audio entirely instead of keeping a resumable pause token for that track.
- Track routing is currently coarse-grained: `ALL`, `FE`, and `IG` only.
- The overlay lists tracks but does not manage playlist metadata or `[trax]` assignments directly.
- In-game movie muting is exposed in `Actions` and persisted as `[config] ingame_movie_muting`; fresh configurations enable it by default.
- The retained Experimental menu entry point is empty and unused; it is not the location for current controls.
- Static BASS startup errors use the active overlay locale while preserving dynamic Windows/BASS diagnostics; UTF-8 text is converted through `MessageBoxW`.
- Runtime filenames and deployment expectations are compatibility-sensitive.

## Source-of-Truth File Map for Future Changes

Use this file map when scoping work:

- Game-state meaning: `src/app/defs.hpp`
- Attach flow and NFSU2 patch sites: `src/app/main.cpp`
- Chyron gating and package checks: `src/app/hook/hook.hpp`
- Audio behavior and playlist flow: `src/app/audio/audio.cpp`
- BASS dynamic loading boundary: `src/app/audio/bass_api.cpp`
- Persistent diagnostics and rotation: `src/utils/logger/logger.hpp`
- Playback metadata extraction: `src/app/audio/player.cpp`
- Hotkey behavior and rebinding safety: `src/app/input/input.cpp`
- Localization parsing and active bundle selection: `src/app/localization/localization.cpp`
- Overlay behavior and release notice logic: `src/app/menus/menus.cpp`
- INI creation, migration, and persistence: `src/app/settings/settings.cpp`
- Build outputs and naming: `lua/windows.lua`
- User-facing product description: `README.md`
- Configuration contract: `CONFIGURATION.MD`
- Build contract: `BUILDING.md`

## Reliability Checklist for Contributors and Agents

Before changing playback, hooks, or settings, verify all of the following:

- Which `GameFlowState` values are affected.
- Whether the change alters playlist context selection.
- Whether the change alters loading-screen stop behavior.
- Whether the change changes when `audio::pause()` or `audio::play()` is reached.
- Whether chyron visibility remains correct when pausing and resuming.
- Whether the overlay and hotkeys still call the same shared helpers.
- Whether any hotkey rebinding edge case can cause live actions during capture.
- Whether configuration persistence and migration still match the runtime behavior.
- Whether translation bundles, `[config] language`, placeholder fallback, and next-frame locale switching remain aligned.
- Whether the runtime filenames, `bass.dll` expectations, and deployment layout remain stable.

For audio-related work, the most failure-prone regression areas are:

- frontend to in-game transitions,
- loading-screen behavior,
- first-chyron startup lock behavior,
- pause and resume coherence,
- shuffle history correctness,
- persistence of new runtime toggles.
