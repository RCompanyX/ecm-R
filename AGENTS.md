# ECM-R Agent Instructions

### 1. Dev Commands
- No CMake.
- `.\generate.bat` → VS 2022 sol (Premake).
- Build: `msbuild build\ECM-R.sln /t:Overlay /p:Configuration=Release /p:Platform=Win-x86 /m`. (Target Win-x86 ONLY).

### 2. Build/Deploy
- Target: NFSU2 (Win-x86).
- Out: `build/bin/Release-Win-x86/x86/ecm-r.x86.dll` (Premake `Overlay` target auto-copies to `.asi` via post-build in `lua/windows.lua`).
- INI: `ecm-r.x86.ini` (local, hardcoded in `settings::config_file`, `src/app/settings/settings.cpp`).
- VERSION: `VERSION` macro in `src/app/stdafx.hpp` (e.g. `"v0.5.13-alpha"`). Source of truth for semver; bumped only by `ecmr-release`.
- BASS: Load dynamic (`LoadLibraryA`/`GetProcAddress`). No bin commit, no header reference. Logic: `src/app/audio/bass_api.*`.
- Deploy: `ecm-r.x86.asi`, `ecm-r.x86.ini`, `bass.dll`.

### 3. Quirks
- Audio/ImGui Init: Hook frame 1 (e.g., `hkEndScene`), not `main.cpp`.
- Hotkeys: Locked during startup banner (`audio::are_hotkeys_locked()` ← `first_chyron_completed`).
- Pause State: `manual_paused` | `game_paused` (union → `audio::paused`).
- Notification: Hide (no empty text). Chyron gated by `hook::SummonChyron()` state rules.
- `ingame_movie_muting=true`: `IG_PlayMovie.fng` trigger → reconcile FNG @ `audio::update()` and `sync_game_pause_from_mute_packages()`.
- Crashdump: `CustomUnhandledExceptionFilter` in `main.cpp` writes `ecm-r-YYYYMMDDHHMMSS.dmp` next to the plugin and `MessageBoxA`s the path.
- Shutdown: `global::shutdown=true` disables audio for the session (BASS load fail, device init fail, version mismatch).

### 4. Context Rules
- State: `GameFlowState` @ `0x008654A4` (`src/app/defs.hpp`).
- FE Context: no `IG` songs. IG Context: no `FE` songs.
- Nav: Use `play_next_song()`/`play_previous_song()` (no boolean direction flags).
- Shuffle: Bounded history.

### 5. Work Roles
- Route by work type, not subsystem (repo tightly coupled):
  - **Viability/planning**: assess fit/risk, reject bad scope, plan approved work. Plan output must include CHANGELOG.md entries under `## [Unreleased]` — add to existing section or create new subsection headers (### Added, ### Changed, ### Fixed, etc) as needed.
  - **Evolutive**: build approved features/UX/config/docs. Keep CHANGELOG.md `## [Unreleased]` entries updated as work progresses (add/change/fix entries, not just at release time).
  - **Incidents**: reproduce bug, isolate regression, fix, validate frontend/loading/racing/overlay/config.

### 6. Pre-Work Checks
- Non-trivial work: read `docs/application-context.md`, then confirm in code.
- State/playback changes: check playlist context, pause/resume, loading screens.
- Playback work: review hooks/mute triggers in `src/app/main.cpp` and `src/app/hook/hook.hpp`, incl FNG packages.
- Hook inventory in `src/app/main.cpp`: FE/IG music volume patch (`0x0083AA30`/`0x0083AA34`), save-load audio skip (`0x00534535`), menu slider disable (`0x004B6EDA`, `0x004C347B`), `sys_init` jump (`0x0057EDA3`), `NFSU2_MainLoop` (`0x005811E4`, calls `audio::update`), package-load MinHook (`0x00537980` → `sub_00537980`, drives mute pause/resume). State read via `game_state` @ `0x008654A4` (`src/app/defs.hpp`).

### 7. Code Rules
- Keep state detection, overlay controls, playlist filters (ALL/FE/IG), config persistence aligned.
- Audio/playlist: inspect state transitions first, then verify frontend/loading/in-race/overlay.
- Keep runtime names, config keys, deployment stable unless task requires change.
- Prefer small coherent changes preserving NFSU2 events, hook addresses, playback state transitions.

### 8. Validation
- Build: `generate.bat` → VS solution → Release | Win-x86.
- Audio: validate BASS runtime loading + screen transitions + playback + overlay controls.
- Viability output: list affected states, audio transitions, hooks, overlay flows, persisted settings.

### 9. Docs
- Main: `README.md`, `BUILDING.md`, `CONFIGURATION.MD`, `docs/application-context.md`, `docs/releases/vX.Y.Z-alpha.md`, `CHANGELOG.md`.
- Docs match actual config names, runtime filenames, deploy paths.
- Release notes: target release changes only, match GitHub format, keep section pattern.

### 10. Workflow
- Language: English.
- Branch: `dev_<slug>` (lowercase ASCII, words joined by `_` or `-`, no spaces, ≤ ~40 chars). Create before any edit (no pre-create confirmation gate; collisions are the only stop condition — see `ecmr-dev` branch policy).
- Copyright/attribution: Preserve ECM-R / ECM / BttrDrgn lineage.
- **Git commits: never commit without explicit user approval.** Before any `git commit`, `git push`, or GitHub write operation, you must ask the user and receive confirmation. Informational/read-only Git/GitHub operations (status, diff, log, read) do not require approval.

### 11. Project Agents

ECM-R uses specialized subagents managed through OpenCode. Route work to the correct agent type:

#### `@ecmr-explore` — Idea Explorer (entry point)
- **Mode:** Primary. Read-only (edit/bash denied). Never edits files or runs builds.
- **Purpose:** Receive raw ideas (feature/bug/enhancement) → assess feasibility against the codebase → classify `VIABLE` / `NOT_VIABLE` / `NEEDS_CLARIFICATION` → delegate viable ideas to `@ecmr-plan` via Task tool with the full feasibility assessment.
- **Output:** Structured feasibility document (verdict, affected subsystems, affected GameFlowStates, risk assessment, complexity, prerequisites).

#### `@ecmr-plan` — Planning Agent
- **Mode:** Read-only. Never edits files or runs builds.
- **Purpose:** Receive feature/bug requests → analyze viability and risk → produce implementation plan covering: affected GameFlowStates, audio transitions, hooks, overlay flows, persisted settings.
- **Output:** Concise plan document.
- **Delegation:** After plan is approved, delegates execution to `@ecmr-dev` via Task tool with full plan text.

#### `@ecmr-dev` — Developer Agent
- **Mode:** Read-write. Creates branches, edits code, runs builds.
- **Purpose:** Execute approved plans from `@ecmr-plan`. Implement features, fix bugs, update docs.
- **Rules:** Follows AGENTS.md (all 10 sections) + `docs/application-context.md`. Creates `dev_...` branch before any edit. Builds `Release | Win-x86`. Updates CHANGELOG.md `## [Unreleased]` as work progresses.
- **Stack:** C++17, Premake, ImGui, BASS, MinHook.

#### `@ecmr-release` — Release Agent
- **Purpose:** Manage version bumps and release packaging.
- **Workflow:** Bump semver in `stdafx.hpp` → rename CHANGELOG `[Unreleased]` → generate `docs/releases/vX.Y.Z-alpha.md` → sync README/CONFIGURATION/BUILDING.
- **Restrictions:** Never touches `docs/application-context.md` (owned by dev agent), `src/` (except `stdafx.hpp`), `.opencode/`, or `tools/`.

#### Agent workflow
Typical task flow: `ecmr-explore` (assess) → `ecmr-plan` (plan) → `ecmr-dev` (implement) → `ecmr-release` (release).

`ecmr-explore`, `ecmr-plan`, and `ecmr-dev` load the caveman skill and read AGENTS.md + `docs/application-context.md` on every conversation.
