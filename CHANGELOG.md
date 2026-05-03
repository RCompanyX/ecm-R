# Changelog

All notable changes to ECM-R are documented in this file.

This changelog currently tracks the tagged releases recorded in this repository.

## [v0.5.6-alpha] - 2026-05-03

### Added
- Added manual Pause/Resume playback control in the overlay.
- Added configurable `pause_track` key binding with default `F8`.
- Added a playback state indicator in the main overlay bar so the current `Paused` or `Playing` state is visible at a glance.
- Added an About menu in the overlay with fork attribution plus repository and issue tracker links.

### Changed
- Manual pause now persists across frontend and in-game context changes until the user resumes playback.
- Resuming playback now reuses the paused track only when it is still valid for the current playlist context (`ALL`, `FE`, `IG`).
- If the paused track is no longer valid after a context change, resume continues with the next valid track using the current shuffle and repeat settings.
- Pausing playback now hides the in-game chyron notification, and resuming playback shows it again for the active track.

### Documentation
- Updated the README to document pause/resume controls, the new `pause_track` hotkey, overlay status display, and the About menu.
- Updated the configuration manual to document the new `pause_track` entry and manual pause behavior.

## [v0.5.5-alpha] - 2026-04-28

### Added
- Added separate `frontend_volume` and `ingame_volume` settings for context-based playback volume control.
- Added context-aware volume handling that automatically switches between frontend and in-game playback states.
- Added overlay volume controls that prioritize the active context and keep the secondary context available.

### Changed
- Volume is now applied per playback channel using `BASS_ATTRIB_VOL` instead of the previous global stream volume path.
- Existing configurations are automatically migrated to include `frontend_volume` and `ingame_volume`, using legacy `volume` as the fallback source.
- Volume is reapplied when a new song starts, when the game context changes, and when the user adjusts volume from the overlay.

### Documentation
- Updated the README and configuration guidance to document context-based volume control.
- Removed **Context-Based Volume Control** from planned features because it is now implemented.

## [v0.5.4-alpha] - 2026-04-27

### Added
- Added **Previous Track** control to navigate backward through the playlist.
- Added configurable `previous_track` key binding (default `F9`).
- Added **Previous** button to the overlay Actions menu.
- Added playback history tracking for shuffle mode (limited to 50 entries) to ensure previous returns to the actually-played track.
- Support for previous track navigation respects playlist context filtering (`ALL`, `FE`, `IG`) and repeat mode.

### Changed
- Refactored next/previous playlist navigation to use a unified helper function (`play_relative_song()`) for consistency and maintainability.
- Simplified `play_next_song()` and `play_previous_song()` to small wrappers.
- Updated input handling to share common logic between forward and backward navigation.
- Enhanced shuffle mode with playback history tracking (in-memory, max 50 entries) for intuitive previous-track navigation.
- Playback history is automatically cleared when switching to sequential mode to avoid interference with position-based navigation.

### Documentation
- Updated README to include the new `previous_track` key binding and Previous button in the overlay.
- Removed **Previous Track Control** from Planned Features as it is now implemented.

## [v0.5.3-alpha] - 2026-04-26

### Changed
- BASS is now loaded dynamically at runtime from `bass.dll` placed next to the ECM-R runtime files.

### Documentation
- Updated installation and build documentation to require downloading the official `bass.dll` and note testing with BASS `v2.4.18.11`.

## [v0.5.2-alpha] - 2026-04-26

### Added
- Added configurable `shuffle_enabled` and `repeat_enabled` playlist options.
- Added overlay controls to toggle shuffle and repeat modes at runtime.

### Changed
- Playlist playback can now run in sequential mode when shuffle is disabled.
- Playlist looping can now be disabled so playback stops after the last valid track.
- The generated `ecm-r.x86.ini` file now persists shuffle and repeat settings.

### Documentation
- Updated the changelog for the new playlist playback options.
- Updated project documentation to describe the new configuration entries and overlay behavior.

## [v0.5.1-alpha] - 2026-04-25

### Added
- Added the `stop_music_on_loading_screens` configuration option.

### Changed
- Updated runtime and configuration filenames to the `ecm-r.*` naming scheme.
- Improved configuration version handling.

### Documentation
- Updated documentation for the new loading screen music behavior.
- Refreshed general project documentation.

## [v0.5.0-alpha] - 2026-04-24

### Added
- First tagged ECM-R alpha release.
- Added configurable key bindings for overlay toggle and track skipping.
- Added build documentation for generating the plugin.

### Changed
- Rebranded the fork to ECM-R.
- Improved playback behavior across game phases.
- Added persistent volume saving.
- Updated project packaging and ignored user-specific or generated files.

### Documentation
- Updated the README and setup guidance.
- Clarified the `bass.dll` runtime requirement and BASS licensing notes.
- Preserved fork attribution to the original ECM project by BttrDrgn.

[v0.5.0-alpha]: https://github.com/RCompanyX/ecm/releases/tag/v0.5.0-alpha
[v0.5.1-alpha]: https://github.com/RCompanyX/ecm/releases/tag/v0.5.1-alpha
[v0.5.2-alpha]: https://github.com/RCompanyX/ecm/releases/tag/v0.5.2-alpha
[v0.5.3-alpha]: https://github.com/RCompanyX/ecm/releases/tag/v0.5.3-alpha
[v0.5.4-alpha]: https://github.com/RCompanyX/ecm/releases/tag/v0.5.4-alpha
[v0.5.5-alpha]: https://github.com/RCompanyX/ecm/releases/tag/v0.5.5-alpha
