# ECM-R Configuration Manual

## Overview

ECM-R automatically creates `ecm-r.x86.ini` next to the plugin DLL on its first launch. 

The active runtime is `ecm-r.x86.asi`, and the configuration file must reside in the same directory as `ecm-r.x86.ini`.

### Default Configuration Structure

```ini
[core]
playlist = "Music"
volume = "100"
frontend_volume = "100"
ingame_volume = "100"
version = "..."

[config]
shuffle_enabled = true
repeat_enabled = true
stop_music_on_loading_screens = true

[experimental]
ingame_movie_muting = false

[keys]
toggle_overlay = F11
pause_track = F8
previous_track = F9
skip_track = F10
toggle_shuffle = None
toggle_repeat = None

[trax]
song1.mp3 = ALL
song2.mp3 = FE
song3.mp3 = IG
```

---

## `[core]` Settings

### `playlist`
- **Description:** Name of the folder where the custom music library is stored.
- **Default:** `Music`
- **Behavior:** ECM-R scans this directory relative to the plugin's location.

**Expected Runtime Layout:**
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

### `volume`
- **Description:** Legacy/fallback volume setting.
- **Default:** `100`
- **Range:** `0` to `100`
- **Behavior:** Acts as a global reference and fallback value. If dedicated frontend or in-game volume settings are missing, they automatically inherit this value.

### `frontend_volume`
- **Description:** Playback volume applied when navigating the Main Menu or safehouse menus (Frontend).
- **Default:** `100`
- **Range:** `0` to `100`
- **Behavior:** Reconciled immediately during context transitions. If not present in the INI, it inherits the value of `volume`.

### `ingame_volume`
- **Description:** Playback volume applied while racing or exploring the open world (In-Game).
- **Default:** `100`
- **Range:** `0` to `100`
- **Behavior:** Reconciled immediately during context transitions. If not present in the INI, it inherits the value of `volume`.

### `version`
- **Description:** Tracks the internal mod version.
- **Behavior:** Updated automatically by the mod at startup to coordinate migrations.

---

## `[config]` Settings

### `shuffle_enabled`
- **Description:** Randomizes song playback.
- **Default:** `true`
- **Behavior:** Uses a bounded playback history (up to 50 tracks) to prevent repeating recently played tracks and to support navigating backward (`previous_track`) and forward (`skip_track`) reliably while shuffling.

### `repeat_enabled`
- **Description:** Loops playback.
- **Default:** `true`
- **Behavior:** Controls whether playback returns to the start of the playlist queue once all valid tracks for the current game context have finished.

### `stop_music_on_loading_screens`
- **Description:** Suspends music during load screens.
- **Default:** `true`
- **Behavior:** Halts the active track on loading screens. If set to `false`, music plays uninterrupted across screens.

---

## `[experimental]` Settings

### `ingame_movie_muting`
- **Description:** Resolves audio conflicts during comic or story movie sequences.
- **Default:** `false`
- **Behavior:** When set to `true`, the game treats `IG_PlayMovie.fng` as a mute trigger. The plugin reconciles the active pause state dynamically against loaded interface packages. This prevents background music from clashing with comic voice-overs and ensures playback resumes only after the movie finishes.

### Accepted Boolean Formats
All `[config]` and `[experimental]` boolean parameters accept:
- **True values:** `true`, `1`, `yes`, `on`
- **False values:** `false`, `0`, `no`, `off`

---

## `[keys]` Settings

### Hotkey Rules
- **Startup Lock:** Keybindings are temporarily locked during the initial startup music banner (first chyron) and unlock automatically after the banner is dismissed.
- **Duplicate Protection:** The in-game UI rejects duplicate key assignments. If duplicates are forced manually via the INI, the mod automatically restores conflicting bindings to their defaults at load time.
- **Disabling Keys:** Assign a key to `None` or `Unbound` to disable it.

### Parameters
| Setting Name | Default Value | Description |
|---|---|---|
| `toggle_overlay` | `F11` | Shows/hides the ImGui menu overlay. (Cannot be unbound via UI to prevent losing access). |
| `pause_track` | `F8` | Manually pauses/resumes music. Preserved until manually unpaused or context becomes invalid. |
| `previous_track` | `F9` | Navigates to the prior track (navigates history when shuffled). |
| `skip_track` | `F10` | Skips to the next track in the queue. |
| `toggle_shuffle` | `None` | Toggles shuffle mode at runtime and rebuilds the playlist order. |
| `toggle_repeat` | `None` | Toggles playlist repeat mode at runtime. |

### Supported Key Names
The following virtual-key mappings are recognized:
- **Function Keys:** `F1` through `F24`
- **Alphanumeric:** `A` to `Z`, `0` to `9`
- **Navigation & Editing:** `Space`, `Tab`, `Enter`, `Esc`, `Escape`, `Backspace`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`
- **Arrows:** `Up`, `Down`, `Left`, `Right`
- **None State:** `None`, `Unbound`

---

## `[trax]` Settings

Every scanned track is registered under `[trax]`. ECM-R automatically adds newly discovered tracks with `ALL` routing and removes orphaned entries when files are deleted from the playlist folder. Track routing **must be configured manually** by editing `ecm-r.x86.ini` if you want to change from `ALL` to `FE` or `IG`, as the in-game overlay playlist menu is read-only.

### Routing Contexts
- `ALL`: Play everywhere.
- `FE`: Play only in Frontend (main menu, safehouse).
- `IG`: Play only In-Game (races, free roam).

*Note: Invalid/unrecognized routing tokens default automatically to `ALL`.*

---

## Behavior Notes

1. **Auto-Save:** Modifying shuffle, repeat, volumes, or hotkeys within the ImGui overlay writes updates to `ecm-r.x86.ini` immediately.
2. **Transient State:** Manual pause state is held in memory and resets when the game is closed.
3. **Binding Interlock:** While the `Hotkeys` menu is actively waiting for key capture input, all hotkey trigger actions are suspended.
4. **Context Reconciliation:** 
   - Frontend context strictly excludes `IG`-assigned tracks.
   - In-Game context strictly excludes `FE`-assigned tracks.
   - If game context transitions while manually paused, playback remains paused unless the current track is invalid for the new context (which triggers a skip to the next valid song).
5. **Supported Audio Containers:** `.wav`, `.mp1`, `.mp2`, `.mp3`, `.ogg`, and `.aif`.
6. **Title Parsing:** Filenames are split on the first `-` character with whitespace trimmed from both sides. If a file matches `Artist - Title.ext`, the chyron, overlay menu bar, and playlist menu show the parsed metadata as clean `Artist - Title` formatting automatically.
