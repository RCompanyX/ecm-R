# ECM-R Configuration Manual

## Overview

ECM-R creates `ecm-r.x86.ini` automatically on first launch.

Default configuration:

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

## `[core]` Settings

### `playlist`

- Defines the folder name used as the music library.
- Default value: `Music`.
- ECM-R looks for this folder relative to the mod location.

Expected runtime layout:

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

- Legacy fallback volume used when a specific context volume is unavailable.
- Range: `0` to `100`.
- Default value: `100`.

### `frontend_volume`

- Volume used while ECM-R is playing in frontend or menu contexts.
- Range: `0` to `100`.
- Default value: `100`.
- Existing configurations inherit the value from `volume` when this setting is missing.

### `ingame_volume`

- Volume used while ECM-R is playing during in-game or racing contexts.
- Range: `0` to `100`.
- Default value: `100`.
- Existing configurations inherit the value from `volume` when this setting is missing.

### `version`

- Internal version marker managed by ECM-R.
- This value is updated automatically by the mod.

## `[config]` Settings

### `shuffle_enabled`

- Controls whether valid tracks are shuffled before playback.
- Default value: `true`.
- When set to `false`, ECM-R plays valid tracks in discovered order.

### `repeat_enabled`

- Controls whether playback restarts from the beginning of the valid track list after the last song finishes.
- Default value: `true`.
- When set to `false`, playback stops after the last valid track in the current context.

### `stop_music_on_loading_screens`

- Controls whether ECM-R stops the current song during loading screens.
- Default value: `true`.
- When set to `false`, custom music continues through loading screens.

## `[experimental]` Settings

### `ingame_movie_muting`

- Controls an experimental mute path for comic-style in-game movie sequences.
- Default value: `false`.
- When set to `true`, ECM-R also treats `IG_PlayMovie.fng` as a mute-trigger package.
- This experimental mode also switches the package hook to reconcile pause state against the currently loaded mute packages, which avoids resuming too early while the movie package is still active.
- When left at `false`, ECM-R keeps the legacy package-hook behavior.

### Accepted boolean values

The `[config]` and `[experimental]` sections accept these values:

- `true`
- `false`
- `1`
- `0`
- `yes`
- `no`
- `on`
- `off`

## `[keys]` Settings

- ECM-R hotkeys remain locked during the first startup music banner and unlock automatically after that first chyron has been shown and dismissed once.
- The overlay `Hotkeys` menu applies and saves runtime binding changes immediately.
- Use `None` or `Unbound` to leave a binding disabled.

### `toggle_overlay`

- Key used to show or hide the in-game overlay.
- Default value: `F11`.
- The overlay UI does not offer a `Clear` button for this action, so you do not accidentally strand yourself without a way to reopen the menu.

### `previous_track`

- Key used to go back to the previous song.
- Default value: `F9`.

### `pause_track`

- Key used to pause or resume ECM-R playback manually.
- Default value: `F8`.
- While pause is active, ECM-R keeps the pause state until the user resumes playback.
- If the game context changes while paused, ECM-R resumes the same track only if it is still valid for the new context. Otherwise, it continues with the next valid track for the current context.

### `skip_track`

- Key used to jump to the next song.
- Default value: `F10`.

### `toggle_shuffle`

- Key used to toggle shuffle mode from the keyboard.
- Default value: `None`.
- When enabled from the keyboard, ECM-R uses the same runtime behavior as the overlay checkbox: it updates `shuffle_enabled`, rebuilds the playlist order, and resets the current playlist index.

### `toggle_repeat`

- Key used to toggle repeat mode from the keyboard.
- Default value: `None`.
- This action uses the same runtime behavior as the overlay checkbox and persists the updated repeat state immediately.

### Supported key values

ECM-R accepts:

- Function keys such as `F1` to `F24`
- Letters such as `A` to `Z`
- Digits such as `0` to `9`
- Named keys such as `Space`, `Tab`, `Enter`, `Esc`, `Escape`, `Backspace`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Up`, `Down`, `Left`, and `Right`
- `None` or `Unbound` for an explicit disabled state

If a key value is invalid or duplicates another ECM-R action, ECM-R restores that action to its built-in default on the next load. Duplicate assignments are rejected directly in the overlay.

## `[trax]` Settings

Each discovered song is added under `[trax]`.

Allowed values:

- `ALL`: Play everywhere
- `FE`: Frontend only
- `IG`: In-game only

This makes it possible to separate menu music from racing music.
Invalid or missing values fall back to `ALL`.

## Behavior Notes

- Changes made from the overlay to shuffle, repeat, volume, or hotkey settings are saved back to `ecm-r.x86.ini` automatically.
- Manual pause state is not persisted across game restarts.
- While the `Hotkeys` menu is waiting for a new binding, ECM-R suspends all ECM-R hotkey actions until the user binds a key or clicks `Cancel`.
- While manual pause is active, the in-game chyron is hidden and is shown again when playback resumes.
- If a filename follows the format `Artist - Title.ext`, the overlay chyron uses that information when possible.
- Supported playlist formats are `.wav`, `.mp1`, `.mp2`, `.mp3`, `.ogg`, and `.aif`.
