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
  - **Viability/planning**: assess fit/risk, reject bad scope, plan approved work.
  - **Evolutive**: build approved features/UX/config/docs.
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
