# ECM-R Agent Instructions

### 1. Dev Commands
- No CMake.
- `.\generate.bat` → VS 2022 sol (Premake).
- Build: `msbuild build\ECM-R.sln /t:Overlay /p:Configuration=Release /p:Platform=Win-x86 /m`. (Target Win-x86 ONLY).

### 2. Build/Deploy
- Target: NFSU2 (Win-x86).
- Out: `build/bin/Release-Win-x86/x86/ecm-r.x86.dll` (copy to `.asi`).
- INI: `ecm-r.x86.ini` (local).
- BASS: Load dynamic (`LoadLibraryA`/`GetProcAddress`). No bin commit, no header reference. Logic: `src/app/audio/bass_api.*`.
- Deploy: `ecm-r.x86.asi`, `ecm-r.x86.ini`, `bass.dll`.

### 3. Quirks
- Audio/ImGui Init: Hook frame 1 (e.g., `hkEndScene`), not `main.cpp`.
- Hotkeys: Locked during startup banner.
- Pause State: `manual_paused` | `game_paused`.
- Notification: Hide (no empty text).
- `ingame_movie_muting=true`: `IG_PlayMovie.fng` trigger → reconcile FNG @ `audio::update()`.

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
- Main: `README.md`, `BUILDING.md`, `CONFIGURATION.MD`, `docs/application-context.md`, `CHANGELOG.md`.
- Docs match actual config names, runtime filenames, deploy paths.
- Release notes: target release changes only, match GitHub format, keep section pattern.

### 10. Workflow
- Language: English.
- Branch: `dev_...`.
- Copyright/attribution: Preserve ECM-R / ECM / BttrDrgn lineage.
- **Git commits: never commit without explicit user approval.** Before any `git commit`, `git push`, or GitHub write operation, you must ask the user and receive confirmation. Informational/read-only Git/GitHub operations (status, diff, log, read) do not require approval.

### 11. Project Agents

ECM-R uses specialized subagents managed through OpenCode. Route work to the correct agent type:

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
Typical task flow: `ecmr-plan` (plan) → `ecmr-dev` (implement) → `ecmr-release` (release).

Both `ecmr-plan` and `ecmr-dev` load the caveman skill and read AGENTS.md + `docs/application-context.md` on every conversation.
