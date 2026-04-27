# v0.5.4-alpha - Previous Track Navigation Control

## Release Summary

ECM-R v0.5.4-alpha adds **Previous Track** navigation functionality with smart playback history tracking in shuffle mode. This release refactors the playlist navigation system for consistency and maintainability while providing an intuitive "back" experience for users.

## What's New

### Previous Track Navigation

- Added **Previous Track** control to skip backward through the playlist.
- New hotkey `F9` (configurable) to navigate to the previous song.
- New **Previous** button in the Actions overlay menu.
- Supports both frontend and in-game contexts with proper context filtering (`ALL`, `FE`, `IG`).
- Respects repeat mode:
  - When repeat is enabled and you're on the first track, pressing previous wraps to the last track.
  - When repeat is disabled, pressing previous on the first track stays on the first track.

### Playback Mode-Specific Behavior

#### Sequential Mode
- **Previous** navigates backward through the discovered track order.
- Uses the static playlist order (no randomization).
- Behavior is predictable and consistent.

#### Shuffle Mode
- **Previous** navigates through actual playback history (last 50 tracks).
- Returns to the song that was **actually played before**, not just the previous position in the randomized list.
- History is tracked automatically and limited to 50 entries to maintain bounded memory usage.
- If you navigate backward and then forward, you can retrace your playback steps.
- When the 50-track limit is reached, the oldest entries are removed.

### Configuration Changes

#### Updated `[keys]` Section

The `ecm-r.x86.ini` now includes:

```ini
[keys]
toggle_overlay = F11
previous_track = F9
skip_track = F10
```

**Supported key values:**
- Function keys: `F1` to `F24`
- Letters: `A` to `Z`
- Digits: `0` to `9`
- Named keys: `Space`, `Tab`, `Enter`, `Escape`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Up`, `Down`, `Left`, `Right`

**Default:**
- `previous_track = F9`

### Backward Compatibility

- Existing configurations automatically gain the `previous_track` key binding on next launch.
- The default binding is set to `F9` to avoid conflicts with existing hotkeys.
- All previous playback features remain unchanged.

## Technical Changes

### Playlist Navigation Refactor

- Unified next/previous navigation into a single shared helper function (`play_relative_song()`) for consistency.
- Introduced internal helpers:
  - `ensure_playlist_order_for_current_context()`: Validates and recreates playlist order if context changed.
  - `move_current_song_index()`: Centralized index movement logic with repeat and boundary handling.
  - `play_relative_song()`: Common navigation function supporting both directions (+1 for next, -1 for previous).
  - `record_playback_history()`: Tracks playback history in shuffle mode (limited to 50 entries).
  - `try_play_song_from_history()`: Retrieves songs from playback history without regenerating order.
- Simplified `play_next_song()` and `play_previous_song()` to small wrappers calling the shared helper.

### Shuffle Mode Enhancements

- Introduced playback history tracking exclusively for shuffle mode.
- Previous track navigates through actual playback history instead of just reversing index position.
- History is automatically cleared when switching to sequential mode.
- Forward navigation after backward movement uses history to retrace steps.
- Memory-bounded history (max 50 entries) prevents unbounded growth.

### Overlay Integration

- Added **Previous** button to the Actions menu, positioned before the **Skip** button.
- Button responds to both keyboard input and mouse clicks.
- Navigation properly respects playlist context and filtering.

### Input Handling

- Extended input system to handle the new `previous_track_key` binding.
- Unified next/previous track triggering through helper functions in input module.
- Both keyboard and overlay actions route through the same internal handlers.

## How to Use

1. **Navigate backward using the default hotkey:**
   - Press `F9` while in-game or in the frontend to skip to the previous song.

2. **Navigate backward using the overlay:**
   - Open the ECM-R overlay (`F11` by default).
   - Go to **Actions**.
   - Click the **Previous** button to go back one track.

3. **Change the previous track hotkey:**
   - Open `ecm-r.x86.ini`.
   - Edit `previous_track` under `[keys]`.
   - Restart the game.

## Behavior Examples

### Sequential Mode (Shuffle Disabled)

```
Playlist Order: [Song A, Song B, Song C]
Current: Song B
Action: Press F9 (previous)
Result: Navigates to Song A (position-based)
```

### Shuffle Mode with Repeat Enabled (Default)

```
Playback History: [Song C, Song A, Song B] (randomly played)
Current: Song B (just finished)
Action: Press F9 (previous)
Result: Returns to Song A (the song that was actually played before)
```

```
Playback History: [Song C, Song A, Song B]
Current: Song A (after pressing previous)
Actions: Press F10 (next) → Press F10 (next)
Result: Song B → (new shuffle song D) (forward goes beyond history)
```

### With Repeat Disabled

```
Playlist: [Song A, Song B, Song C]
Current: Song A
Action: Press F9 (previous)
Result: Stays on Song A (cannot go below first track)
```

## Important Notes

- **Sequential Mode:** Previous and next navigate by position in the discovered track order.
- **Shuffle Mode:** Previous navigates through actual playback history (limited to last 50 tracks played), providing a more intuitive "back" experience.
- **Memory:** Playback history is memory-efficient (50 integers max ≈ 200 bytes overhead) and is cleared when switching to sequential mode.
- **Context Filtering:** All navigation modes (`ALL`, `FE`, `IG`) are properly maintained during previous/next operations.
- **Repeat Behavior:** Both modes respect the `repeat_enabled` setting at playlist boundaries.

## Installation

For installation instructions, refer to the [README.md](https://github.com/RCompanyX/ecm-r#installation).

## Known Issues

None reported yet for this release.

## Testing

This release has been tested for:
- ✅ Previous track navigation in sequential mode
- ✅ Previous track navigation in shuffle mode with history tracking
- ✅ Repeat mode behavior at playlist boundaries
- ✅ Context filtering (ALL, FE, IG) during navigation
- ✅ Overlay button and hotkey functionality
- ✅ Configuration persistence

## Commits

- **b65e36f** - docs: enhance v0.5.4-alpha release notes with shuffle history details
- **ac789f5** - feat: add previous track navigation control

## References

- [Full Changelog](https://github.com/RCompanyX/ecm-r/blob/main/CHANGELOG.md)
- [Compare v0.5.3-alpha...v0.5.4-alpha](https://github.com/RCompanyX/ecm-r/compare/v0.5.3-alpha...v0.5.4-alpha)

---

**Status:** Pre-release (Alpha)  
**Tag:** `v0.5.4-alpha`  
**Date:** 2026-04-27

