# Embedded Metadata Tags for Chyron/Overlay

- **Date:** 2026-07-31
- **Status:** Implemented (pending CI build + user commit approval)
- **Branch:** dev_metadata_tags

---

# Implementation Plan: Real Metadata Tags for Chyron/Overlay

## 1. Summary

Read embedded metadata tags (title/artist) from music files across all supported formats, display them in the in-game chyron and overlay. When tags are unavailable or unparseable, silently fall back to the existing filename-derived "Artist - Title" convention. Zero new dependencies, zero new INI keys, zero hook-surface changes.

## 2. Affected Files

| File | Change | Est. lines |
|---|---|---|
| `src/app/audio/bass_api.hpp` | Add `BASS_ChannelGetTags` public wrapper + tag-type constants | +6 |
| `src/app/audio/bass_api.cpp` | Resolve `BASS_ChannelGetTags` export, add wrapper body | +14 |
| `src/app/audio/tags.cpp` | **New.** All tag-parsing logic (ID3v1/v2, Vorbis, RIFF) | ~280 |
| `src/app/audio/audio.hpp` | Declare free function `read_metadata()` (alongside `playing_t`) | +5 |
| `src/app/audio/player.cpp` | Call `read_metadata()` after stream creation, per-field filename fallback | +10 |
| `src/utils/fs/fs.hpp` | Add `ansi_to_utf8()` helper for RIFF INFO tags | +10 |
| `README.md` | Note tag priority over filename convention | +2 |
| `CONFIGURATION.MD` | Note embedded tag support | +2 |
| `CHANGELOG.md` | `### Added` entry under `## [Unreleased]` | +2 |

**No changes:** `defs.hpp`, `main.cpp`, `hook.hpp`, `audio.cpp`, `menus.cpp`, `input.cpp`, `settings.cpp`, `lua/windows.lua` (Premake glob `../src/app/audio/**` auto-picks up `tags.cpp`).

## 3. BASS API Extension (`bass_api.hpp` / `bass_api.cpp`)

### 3.1 New constants in `bass_api.hpp`

```cpp
inline constexpr DWORD bass_tag_id3       = 0;     // ID3v1
inline constexpr DWORD bass_tag_id3v2     = 1;     // ID3v2 (raw block)
inline constexpr DWORD bass_tag_ogg       = 2;     // Vorbis Comments
inline constexpr DWORD bass_tag_riff_info = 0x100; // RIFF INFO chunks
```

### 3.2 New function pointer + export resolution

```cpp
using channel_get_tags_fn = const void*(WINAPI*)(DWORD, DWORD);
```

Resolve in `load()`, null in `reset()`.

### 3.3 Public wrapper

```cpp
/// Returns raw tag data for the given tag type, or nullptr if unavailable.
const void* channel_get_tags(stream_handle_t handle, DWORD tag_type);
```

## 4. New File: `src/app/audio/tags.cpp`

### 4.1 Design

- Anonymous namespace holds all per-format parsers (no header pollution).
- Single public entry point declared in `audio.hpp`:

```cpp
/// Reads embedded title/artist tags from a BASS stream.
/// extension: lowercase file extension (e.g. "mp3", "ogg", "wav").
/// On success, title and/or artist are populated. On failure, both are untouched.
/// Call while stream handle is alive; copies strings before return.
void read_metadata(DWORD stream_handle, const std::string& extension,
                   std::string& title, std::string& artist);
```

- Every parser bounds-checked; parse failure = silent fallback.
- Per-field independence: populate only fields actually found.

### 4.2 ID3v2 (.mp3, .mp1, .mp2; reused for .aif)

1. `BASS_TAG_ID3V2`; nullptr → fall through to ID3v1.
2. Validate 10-byte header (`ID3` magic + version + flags + size).
3. Tag size: v2.4 syncsafe `(b0<<21)|(b1<<14)|(b2<<7)|b3`; v2.3 big-endian; v2.2 skip.
4. Skip extended header if flags & 0x40.
5. Iterate frames (4-byte ID + 4-byte size + 2-byte flags), bounds-checked; v2.3 size big-endian, v2.4 syncsafe. `TIT2`/`TPE1` only; stop when both found.
6. Encoding byte: 0x00 ISO-8859-1 → UTF-8; 0x01 UTF-16+BOM (check BOM endianness) → WideCharToMultiByte(CP_UTF8); 0x02 UTF-16BE → swap then convert; 0x03 UTF-8 verbatim; other skip.
7. Strip trailing nulls/spaces, `logger::trim()`.

**ID3v1 fallback:** `BASS_TAG_ID3` → 128 bytes starting `TAG`; title bytes 3-32, artist bytes 33-62; strip trailing spaces/nulls; Latin-1 → UTF-8; populate only non-empty fields.

### 4.3 Vorbis Comments (.ogg)

`BASS_TAG_OGG` → UTF-8 `KEY=VALUE\0...\0\0`. Walk segments, split on first `=`, case-insensitive `TITLE`/`ARTIST`, copy verbatim, trim.

### 4.4 RIFF INFO (.wav)

`BASS_TAG_RIFF_INFO` → null-terminated key/value pairs (`INAM`=title, `IART`=artist). ANSI → `fs::ansi_to_utf8()`. Trim. Tolerant walker: bail silently on unexpected layout.

### 4.5 AIFF (.aif)

Same ID3v2 parser (BASS exposes ID3v2 for AIFF when present).

## 5. `player.cpp` Integration

1. `title = "N/A"; artist = "N/A";`
2. Extract lowercase extension.
3. `read_metadata(chan[channel], ext, title, artist);` while handle alive.
4. Per-field fallback — filename parse fills ONLY fields still `"N/A"` (never overwrites tag-derived values).
5. Trim, assign `currently_playing`, `request_current_chyron()` (unchanged).

## 6. `fs::ansi_to_utf8()`

CP_ACP → wide (MultiByteToWideChar) → UTF-8. Empty string on failure.

## 7. Zero-change confirmations

- GameFlowStates: none (content-only).
- Hooks: zero.
- Overlay: `menus.cpp:677-679` unchanged, benefits automatically.
- Pause/resume: `audio.cpp:194-197` untouched; `"N/A"` guards in `try_show_current_chyron` still work.
- Settings: no new INI keys, no migration.
- First-chyron lockout, shuffle history, loading-screen stop, hotkeys: untouched.

## 8. Validation

- Build (CI pipeline, `Release | Win-x86`): `generate.bat` → `msbuild build\ECM-R.sln /t:Overlay /p:Configuration=Release /p:Platform=Win-x86 /m`.
- Manual test matrix (22 cases): mp3 ID3v1 / ID3v2.3 Latin-1 / ID3v2.3 UTF-16 / ID3v2.4 UTF-8 / title-only (per-field fallback) / no tags / malformed ID3v2 / non-ASCII tags; ogg full / artist-only / none; wav RIFF INFO / none; aif ID3v2 / none; non-ASCII filenames; FE chyron; IG chyron; loading-screen suppression; overlay now-playing match; pause→resume re-show; shuffle previous history.

## 9. CHANGELOG.md Entry

Under `## [Unreleased]` → `### Added`:

> - Embedded metadata tags (ID3v1, ID3v2, Vorbis Comments, RIFF INFO) are now read from music files and displayed in the in-game chyron and overlay. When tags are unavailable or unparseable, ECM-R falls back to the existing filename-derived "Artist - Title" convention on a per-field basis (e.g., a tagged title with no artist tag will still pick up the artist from the filename).

## 10. Docs Updates

- README.md: embedded tags take priority over the "Artist - Title" filename convention; per-field fallback.
- CONFIGURATION.MD: automatic tag reading, no configuration needed, filename fallback.

## 11. Risk Register

| Risk | Mitigation |
|---|---|
| Malformed ID3v2 causes crash | Bounds-checked parsers; failure = silent fallback |
| v2.3/v2.4 size decode error | Version byte gates decode method |
| UTF-16 BOM endianness | BOM checked before conversion; LE default |
| RIFF INFO garbage | `ansi_to_utf8` failure → empty → fallback |
| Chyron buffer overflow | Tags rarely exceed filename lengths already handled |
| Stale tag pointer | Strings copied immediately while stream alive |
| bass.dll lacks export | Present since BASS 1.8; missing export = existing shutdown path |
