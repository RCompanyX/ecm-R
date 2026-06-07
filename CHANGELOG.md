# Changelog

All notable changes to ECM-R are documented in this file.

This changelog tracks the tagged releases recorded in this repository.

## [Unreleased]

### Documentation
- Documented project subagents (`ecmr-plan`, `ecmr-dev`, `ecmr-release`) in AGENTS.md and `docs/application-context.md`.

### Fixed
- Fixed brief game freeze when changing songs caused by unnecessary `BASS_STREAM_PRESCAN` flag that performed blocking file pre-scan during stream creation.
- Fixed filename parsing to split on the first `-` character and trim whitespace from both sides, correctly handling filenames with extra spaces around the separator (e.g. `04.    -   Song Test One.mp3` → `04. - Song Test One`).
- Fixed artist and title not being trimmed after parsing the filename in the overlay, playlist menu, and chyron notification.
- Fixed the overlay menu bar only displaying the song title; it now also shows the artist in `Artist - Title` format when available, matching the in-game chyron behavior.
- Fixed the playlist menu showing raw filenames with extra whitespace instead of cleaned `Artist - Title` display.

## [v0.5.12-alpha] - 2026-06-06

### Added
- Added Unicode filename support so MP3 files with Cyrillic, Korean, Japanese, or other non-ASCII characters no longer crash the game on startup.

### Changed
- Internal file paths are now stored as UTF-8, with conversion to wide strings at filesystem and BASS API boundaries.
- Directory enumeration uses wide-string `std::filesystem` APIs for correct handling of non-ASCII paths.
- BASS stream creation now passes file paths as wide strings with the `BASS_UNICODE` flag.
- The in-game overlay playlist menu now loads Cyrillic glyph ranges so Unicode filenames render correctly.

### Fixed
- New music files added to the playlist folder now persist to the `[trax]` section of the INI on the next startup instead of being playable only in memory. Previously tracks only appeared in `[trax]` when the configuration was rewritten due to version changes or missing settings keys.
- Orphaned `[trax]` entries (for files no longer on disk) are now automatically cleaned up on startup.
- Fixed a crash on startup when any audio file has non-ASCII characters in its name.
- Fixed the file extension filter that was incorrectly accepting all file types regardless of the configured extension list. Extensions are now matched case-insensitively.
- Fixed the title parser that corrupted some track titles by trimming the first two characters.
- Fixed filenames with multiple ` - ` separators (e.g. `Artist - Title - Subtitle`) showing as `N/A` in the notification.
- Fixed `fs::exists()` to use wide-string filesystem APIs for consistency with the rest of the path handling.

### Known Limitations
- The in-game notification (chyron) shows `?` for characters outside the game's ANSI code page. This is a limitation of the NFSU2 engine itself.

## [v0.5.11-alpha] - 2026-05-24

### Added
- Added an `Experimental` overlay menu with a runtime toggle for in-game movie muting.

### Changed
- Added experimental muting for comic-style in-game movie sequences that use NFSU2's `IG_PlayMovie.fng` package.
- Moved the movie-muting toggle to the new `[experimental]` `ingame_movie_muting` setting, which stays disabled by default.
- Existing temporary `experimental_ingame_movie_muting` entries now migrate to the new setting name and section.
- The legacy package pause/resume behavior remains unchanged while the experimental toggle is disabled.

### Documentation
- Updated the README, configuration manual, and application context to describe the experimental movie-muting flow.
- Added release notes for `v0.5.11-alpha`.

## [v0.5.10-alpha] - 2026-05-10

### Added
- Added a startup-only overlay release notice beside `About` when GitHub reports a newer non-draft release.
- Added explicit `stable release` versus `testing pre-release` wording in the release notice and its tooltip.

### Changed
- Release discovery now reads the GitHub releases list instead of `/releases/latest`, which keeps the repository's published pre-release flow visible.
- The version check now runs once during overlay initialization on an asynchronous task instead of being tied to per-frame UI updates.
- Short WinHTTP timeouts keep unreachable networks from blocking the overlay, and failed checks stay silent by hiding the release notice.
- Version comparison now considers numeric components and prerelease suffixes before surfacing a newer tag.

### Documentation
- Updated the README to document the startup release notice and its stable/testing wording.
- Added release notes for `v0.5.10-alpha`.

## [v0.5.9-alpha] - 2026-05-10

### Changed
- Improved `bass.dll` load failure reporting so the startup popup now shows clearer setup guidance, the localized Windows error text, and the exact path that ECM-R tried to load.
- The runtime BASS loader now captures the original Windows load error before resetting its internal state, which keeps the reported failure reason stable.

### Documentation
- Updated the README roadmap with planned in-game movie handling for comic-style in-game cinematics.
- Updated the README notes to document the new `bass.dll` failure popup details.
- Added release notes for `v0.5.9-alpha`.

## [v0.5.8-alpha] - 2026-05-10

### Added
- Added a `Hotkeys` overlay menu for runtime rebinding of `toggle_overlay`, `pause_track`, `previous_track`, `skip_track`, `toggle_shuffle`, and `toggle_repeat`.
- Added keyboard hotkeys for shuffle and repeat toggles, both disabled by default through `toggle_shuffle = None` and `toggle_repeat = None`.

### Changed
- Hotkey capture now suspends ECM-R hotkey execution until the new binding is confirmed or canceled.
- Duplicate hotkey assignments are rejected in the overlay, and duplicate or invalid key entries are normalized back to safe defaults when the configuration is loaded.
- Existing configurations now migrate the new `toggle_shuffle` and `toggle_repeat` entries automatically.

### Documentation
- Updated the README to document runtime hotkey rebinding, optional shuffle/repeat hotkeys, and `None` / `Unbound` key syntax.
- Updated the configuration manual to document the new `[keys]` entries, duplicate-binding rules, and hotkey capture behavior.

## [v0.5.7-alpha] - 2026-05-05

### Added
- Added the current ECM-R version display to the mod's About menu.

### Changed
- Restored the game's original chyron summon path so startup and resume notifications follow the native NFSU2 flow.
- Tightened frontend FNG resume handling so unrelated frontend package loads no longer retrigger ECM-R playback or re-show the current banner unexpectedly.
- Added deferred chyron retry handling so the current track notification is shown once the required frontend UI packages are ready.
- Locked ECM-R hotkeys during the first startup chyron and unlock them automatically after that first banner has fully disappeared.

### Documentation
- Updated the README to warn that ECM-R hotkeys remain disabled until the first startup banner finishes.
- Updated the configuration manual to document the startup hotkey lock behavior in the `[keys]` section.

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
[v0.5.6-alpha]: https://github.com/RCompanyX/ecm/releases/tag/v0.5.6-alpha
[v0.5.7-alpha]: https://github.com/RCompanyX/ecm/releases/tag/v0.5.7-alpha
[v0.5.8-alpha]: https://github.com/RCompanyX/ecm/releases/tag/v0.5.8-alpha
[v0.5.9-alpha]: https://github.com/RCompanyX/ecm-R/releases/tag/v0.5.9-alpha
[v0.5.10-alpha]: https://github.com/RCompanyX/ecm-R/releases/tag/v0.5.10-alpha
[v0.5.11-alpha]: https://github.com/RCompanyX/ecm-R/releases/tag/v0.5.11-alpha
