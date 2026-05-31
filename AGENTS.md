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

### 3. Quirks
- Audio/ImGui Init: Hook frame 1 (e.g., `hkEndScene`), not `main.cpp`.
- Hotkeys: Locked during startup banner.
- Pause State: `manual_paused` | `game_paused`.
- Notification: Hide (no empty text).
- `ingame_movie_muting=true`: `IG_PlayMovie.fng` trigger → reconcile FNG @ `audio::update()`.

### 4. Context Rules
- State: `GameFlowState` @ `0x008654A4` (`src/app/defs.hpp`).
- FE Context: no `IG` songs. IG Context: no `FE` songs.
- Nav: Use `play_next_song()`/`play_previous_song()`.
- Shuffle: Bounded history.

### 5. Workflow
- Language: English.
- Branch: `dev_...`.
- Copyright: Preserve.
