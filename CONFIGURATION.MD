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
ingame_movie_muting = true
log_level = info
language = en

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

`.ogg` support targets core BASS Ogg Vorbis. Ogg Opus and Ogg FLAC require separate BASS add-ons and are not supported by the current runtime.

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

### `ingame_movie_muting`
- **Description:** Resolves audio conflicts during comic or story movie sequences.
- **Default:** `true` for fresh configurations.
- **Behavior:** When set to `true`, the game treats `IG_PlayMovie.fng` as a mute trigger. The plugin reconciles the active pause state dynamically against loaded interface packages. This prevents background music from clashing with comic voice-overs and ensures playback resumes only after the movie finishes.

### `language`
- **Description:** Selects the overlay language.
- **Default:** `en`
- **Accepted values:** `en` (English) or `es` (neutral Spanish).
- **Behavior:** The active language changes on the next overlay frame and is saved immediately. Invalid or missing values are repaired to `en`.
- **Translation files:** Editable UTF-8 `ecm-r/translations/en.ini` and `ecm-r/translations/es.ini` files are loaded from the plugin-relative namespace at startup. Missing or invalid entries safely fall back to English and compiled defaults. Track metadata, filenames, paths, INI keys, and game chyrons are never translated.

### `log_level`
- **Description:** Selects the persistent diagnostic verbosity.
- **Default:** `info` after the INI is loaded.
- **Accepted values:** `error`, `warning`, `info`, or `debug` (case-insensitive and whitespace-trimmed).
- **Behavior:** Missing or invalid values are repaired to `info`. Bootstrap logging starts unfiltered at `debug` before the INI is read, then applies this value without closing, truncating, or recreating the log.
- **File and retention:** `ecm-r/ecm-r.x86.log` is created alongside the translation bundles, capped at 2 MiB. ECM-R creates the `ecm-r/` folder when missing and validates that an existing path is a directory. When the limit would be exceeded, the active file is moved to `ecm-r/ecm-r.x86.log.1`; the previous `.1` is replaced and no `.2` is created. Older root-level logs are not migrated. If file operations fail, logging continues through the console for that session.

### Accepted Boolean Formats
All `[config]` boolean parameters accept:
- **True values:** `true`, `1`, `yes`, `on`
- **False values:** `false`, `0`, `no`, `off`

### Configuration migration
- The canonical `[config] ingame_movie_muting` value takes precedence.
- If the canonical key is absent, older `[experimental] ingame_movie_muting` and legacy `[config] experimental_ingame_movie_muting` entries migrate to `true`, regardless of their old value.
- If no movie-muting key exists, the value is also `true`. Migrated files are rewritten without the obsolete section and key.
- The old Experimental menu entry point is retained as an empty unused placeholder; movie muting remains canonical under `[config] ingame_movie_muting`.

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
5. **Supported Audio Containers:** `.wav`, `.mp1` (legacy MPEG-1 Layer I compatibility), `.mp2`, `.mp3`, `.ogg`, and `.aif`.
6. **Title Parsing:** ECM-R reads embedded metadata tags (ID3v1, ID3v2, Vorbis Comments, RIFF INFO) from music files when they are available. If tags are missing or unparseable, it falls back to splitting filenames on the first `-` character with whitespace trimmed from both sides. If a file matches `Artist - Title.ext`, the chyron, overlay menu bar, and playlist menu show the parsed metadata as clean `Artist - Title` formatting automatically. This fallback applies on a per-field basis: a tagged title with no artist tag will still pick up the artist from the filename. Long embedded artist and title values are truncated safely for the chyron without splitting UTF-8 characters.
7. **Playback Failure Handling:** Files that BASS cannot open are excluded from playback and the read-only Playlist menu for the current session, while remaining in `[trax]` for re-evaluation on the next launch. Failed attempts are limited to one playlist pass, and startup diagnostics include numeric BASS error codes to help identify stream-open or channel-play failures.
8. **Crash Evidence:** Unhandled bootstrap exceptions write a timestamped `.dmp` beside the module using the raw module path. Crash dumps are independent of the persistent logger and are never automatically deleted or rotated. Release PDBs are archived separately from runtime packages for symbolization.
