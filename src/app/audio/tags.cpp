#include "audio.hpp"
#include "bass_api.hpp"
#include "logger/logger.hpp"
#include "fs/fs.hpp"

#include <algorithm>
#include <cstring>

namespace
{
    // --- local structs matching BASS layouts ---

    // BASS 2.4 TAG_ID3 — packed 128-byte ID3v1 structure.
    // Layout: id[3] + title[30] + artist[30] + album[30] + year[4] + comment[30] + genre.
    struct tag_id3
    {
        char           id[3];
        char           title[30];
        char           artist[30];
        char           album[30];
        char           year[4];
        char           comment[30];
        unsigned char  genre;
    };
    static_assert(sizeof(tag_id3) == 128, "TAG_ID3 packing mismatch against BASS 2.4 layout");

    // --- helpers ---

    // Bounds-checked byte read at ptr+offset; returns 0 if out of bounds.
    std::uint8_t safe_byte(const char* base, std::size_t total, std::size_t off)
    {
        return (off < total) ? static_cast<std::uint8_t>(base[off]) : 0;
    }

    // Reads a big-endian 32-bit value from base+off (4 bytes); returns 0 if OOB.
    std::uint32_t be_u32(const char* base, std::size_t total, std::size_t off)
    {
        if (off + 4 > total) return 0;
        return (static_cast<std::uint8_t>(base[off]) << 24) |
               (static_cast<std::uint8_t>(base[off + 1]) << 16) |
               (static_cast<std::uint8_t>(base[off + 2]) << 8) |
                static_cast<std::uint8_t>(base[off + 3]);
    }

    // Syncsafe integer decode (v2.4) — 4 bytes, 7 bits each.
    std::uint32_t syncsafe_u32(const char* base, std::size_t total, std::size_t off)
    {
        if (off + 4 > total) return 0;
        return (static_cast<std::uint8_t>(base[off]) << 21) |
               (static_cast<std::uint8_t>(base[off + 1]) << 14) |
               (static_cast<std::uint8_t>(base[off + 2]) << 7) |
                static_cast<std::uint8_t>(base[off + 3]);
    }

    // Strip trailing null chars ('\0') and spaces from string in-place.
    void strip_trailing(std::string& s)
    {
        while (!s.empty() && (s.back() == '\0' || s.back() == ' '))
            s.pop_back();
    }

    // Convert ISO-8859-1 (Latin-1) byte range 0x80-0xFF to UTF-8 and append to out.
    void latin1_to_utf8(const char* src, std::size_t len, std::string& out)
    {
        for (std::size_t i = 0; i < len; ++i)
        {
            const unsigned char c = static_cast<unsigned char>(src[i]);
            if (c < 0x80)
            {
                out.push_back(static_cast<char>(c));
            }
            else
            {
                // 0x80-0xFF maps to U+0080-U+00FF → 2-byte UTF-8
                out.push_back(static_cast<char>(0xC0 | (c >> 6)));
                out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }
    }

    // Decode a UTF-16 string to UTF-8. LE if little_endian==true, BE otherwise.
    // Uses memcpy per-element to avoid strict-aliasing UB from reinterpret_cast.
    std::string utf16_to_utf8(const char* data, std::size_t len, bool little_endian)
    {
        if (len < 2) return {};

        const std::size_t num_units = len / 2;
        std::wstring wide;
        wide.reserve(num_units);
        for (std::size_t i = 0; i < num_units; ++i)
        {
            std::uint16_t unit;
            std::memcpy(&unit, data + i * 2, sizeof(unit));
            if (!little_endian)
                unit = static_cast<std::uint16_t>((unit << 8) | (unit >> 8));
            if (unit == 0) break; // null termination
            wide.push_back(static_cast<wchar_t>(unit));
        }

        return fs::wstring_to_utf8(wide);
    }

    // Bounded strlen: returns the number of bytes until NUL or end, whichever comes first.
    std::size_t bounded_strlen(const char* p, const char* end)
    {
        const char* start = p;
        while (p < end && *p != '\0')
            ++p;
        return static_cast<std::size_t>(p - start);
    }

    // ponytail: sanity upper bound for raw BASS tag walkers — not a real buffer length.
    // BASS does not expose a buffer size for these tag strings, so the walker has no way
    // to verify actual bounds. This guard only prevents runaway scans on corrupted data;
    // binary tag APIs remain preferred whenever available.
    inline constexpr std::size_t max_tag_walker_guard = 0x100000; // 1 MiB sanity limit

    // --- parsers ---

    // BASS 2.4 TAG_BINARY — BASS_TAG_ID3V2_BINARY (type 20) returns this struct
    // with the real buffer pointer and its byte length. Requires BASS 2.4.18.3+.
    struct tag_binary
    {
        const void* data;
        DWORD       length;
    };

    void parse_id3v2_block(const char* raw, const std::size_t buf_len,
                           std::string& title, std::string& artist)
    {
        if (raw == nullptr || buf_len < 10) return;

        // First 3 bytes: "ID3"
        if (std::memcmp(raw, "ID3", 3) != 0) return;

        const std::uint8_t version_major = static_cast<std::uint8_t>(raw[3]);
        const std::uint8_t flags          = static_cast<std::uint8_t>(raw[5]);

        // We only handle v2.3 and v2.4.
        if (version_major != 3 && version_major != 4) return;

        // Tag size: v2.4 is syncsafe; v2.3 is big-endian.
        const std::uint32_t declared_tag_size = (version_major == 4)
            ? syncsafe_u32(raw, buf_len, 6)
            : be_u32(raw, buf_len, 6);

        // Declared size must fit within the real buffer (minus 10-byte header).
        if (declared_tag_size > static_cast<std::uint32_t>(buf_len - 10)) return;
        const std::size_t effective_total = (std::min)(buf_len, static_cast<std::size_t>(10) + declared_tag_size);

        // Extended header
        std::size_t cursor = 10;
        if ((flags & 0x40) != 0)
        {
            // Need 4 bytes for extended header size field.
            if (cursor > effective_total || effective_total - cursor < 4) return;
            const std::uint32_t ext_size = (version_major == 4)
                ? syncsafe_u32(raw, effective_total, cursor)
                : be_u32(raw, effective_total, cursor);

            // Validate before adding: cursor + 4 + ext_size must fit within effective_total.
            if (ext_size > static_cast<std::uint32_t>(effective_total - cursor - 4)) return;
            cursor += 4 + ext_size;
            if (cursor >= effective_total) return;
        }

        // O(n) frame scan; bounded by effective_total.
        while (cursor + 10 <= effective_total)
        {
            const char id[5] = { raw[cursor], raw[cursor + 1], raw[cursor + 2], raw[cursor + 3], '\0' };

            // Padding byte or end
            if (id[0] == '\0') break;

            // Frame size: v2.3 uses big-endian, v2.4 syncsafe.
            std::uint32_t frame_size = (version_major == 4)
                ? syncsafe_u32(raw, effective_total, cursor + 4)
                : be_u32(raw, effective_total, cursor + 4);

            cursor += 10; // skip frame header

            // Frame size must not exceed remaining buffer.
            if (cursor > effective_total || frame_size > effective_total - cursor) break;

            const bool is_title  = (std::strcmp(id, "TIT2") == 0);
            const bool is_artist = (std::strcmp(id, "TPE1") == 0);

            if (is_title || is_artist)
            {
                if (frame_size < 2) { cursor += frame_size; continue; }
                const std::uint8_t encoding = static_cast<std::uint8_t>(raw[cursor]);
                const char* frame_data = raw + cursor + 1;
                const std::size_t frame_data_len = frame_size - 1;

                std::string value;
                switch (encoding)
                {
                case 0x00: // ISO-8859-1
                    latin1_to_utf8(frame_data, frame_data_len, value);
                    break;

                case 0x01: // UTF-16 with BOM
                {
                    if (frame_data_len < 2) break;
                    const bool le = (static_cast<std::uint8_t>(frame_data[0]) == 0xFF &&
                                     static_cast<std::uint8_t>(frame_data[1]) == 0xFE);
                    const bool be = (static_cast<std::uint8_t>(frame_data[0]) == 0xFE &&
                                     static_cast<std::uint8_t>(frame_data[1]) == 0xFF);
                    // Skip BOM
                    value = utf16_to_utf8(frame_data + 2, frame_data_len - 2, le || !be);
                    break;
                }

                case 0x02: // UTF-16BE
                    value = utf16_to_utf8(frame_data, frame_data_len, false);
                    break;

                case 0x03: // UTF-8
                    value.assign(frame_data, frame_data_len);
                    break;

                default:
                    break;
                }

                strip_trailing(value);
                logger::trim(value);

                if (is_title && !value.empty() && title == "N/A")
                    title = std::move(value);
                else if (is_artist && !value.empty() && artist == "N/A")
                    artist = std::move(value);

                if (title != "N/A" && artist != "N/A") return; // both found
            }

            cursor += frame_size;
        }
    }

    void parse_id3v2(DWORD handle, std::string& title, std::string& artist)
    {
        const auto* bin = static_cast<const tag_binary*>(
            bass_api::channel_get_tags(handle, bass_api::bass_tag_id3v2_binary));
        if (bin != nullptr && bin->data != nullptr && bin->length >= 10)
        {
            parse_id3v2_block(static_cast<const char*>(bin->data),
                              static_cast<std::size_t>(bin->length), title, artist);
            if (title != "N/A" && artist != "N/A")
                return;
        }

        // Older BASS 2.4 builds expose the ID3v2 block without its length.
        // Use the declared ID3 size inside a conservative scan ceiling so MP2/AIFF
        // remain readable without weakening the bounded binary path above.
        const auto* raw = static_cast<const char*>(
            bass_api::channel_get_tags(handle, bass_api::bass_tag_id3v2));
        parse_id3v2_block(raw, max_tag_walker_guard, title, artist);
    }

    void parse_id3v1(DWORD handle, std::string& title, std::string& artist)
    {
        const auto* raw = static_cast<const char*>(bass_api::channel_get_tags(handle, bass_api::bass_tag_id3));
        if (raw == nullptr) return;

        // BASS_TAG_ID3 returns a pointer to the 128-byte TAG_ID3 structure.
        // Validate the "TAG" magic before accessing fields.
        const auto& tag = *static_cast<const tag_id3*>(static_cast<const void*>(raw));
        if (std::memcmp(tag.id, "TAG", 3) != 0) return;

        // Per-field read helper — don't overwrite a value already filled by ID3v2.
        auto read_field = [](const char* field, std::size_t len, std::string& target)
        {
            if (target != "N/A") return;
            std::string value;
            latin1_to_utf8(field, len, value);
            strip_trailing(value);
            logger::trim(value);
            if (!value.empty())
                target = std::move(value);
        };

        read_field(tag.title,  sizeof(tag.title),  title);
        read_field(tag.artist, sizeof(tag.artist), artist);
    }

    void parse_vorbis(DWORD handle, std::string& title, std::string& artist)
    {
        const auto* raw = static_cast<const char*>(bass_api::channel_get_tags(handle, bass_api::bass_tag_ogg));
        if (raw == nullptr) return;

        // Vorbis comments: "KEY=VALUE\0KEY=VALUE\0...\0\0"
        const char* p = raw;
        const char* const walker_end = p + max_tag_walker_guard; // sanity limit, not real bounds

        // Guard before dereference — condition order matters.
        while (p < walker_end && *p != '\0')
        {
            const char* eq = static_cast<const char*>(std::memchr(p, '=', static_cast<std::size_t>(walker_end - p)));
            if (eq == nullptr)
            {
                // Skip this malformed entry, advance to next \0
                const std::size_t len = bounded_strlen(p, walker_end);
                p += len + 1;
                continue;
            }

            const auto key_len = static_cast<std::size_t>(eq - p);
            std::string key(p, key_len);
            logger::to_upper(key);

            const char* value_start = eq + 1;
            const std::size_t value_max = static_cast<std::size_t>(walker_end - value_start);
            const std::size_t value_len  = bounded_strlen(value_start, value_start + value_max);
            std::string value(value_start, value_len);
            strip_trailing(value);
            logger::trim(value);

            if (!value.empty())
            {
                if (key == "TITLE" && title == "N/A")
                    title = std::move(value);
                else if (key == "ARTIST" && artist == "N/A")
                    artist = std::move(value);
            }

            if (title != "N/A" && artist != "N/A") return;

            p += key_len + 1 + value_len + 1; // key + '=' + value + '\0'
        }
    }

    void parse_riff_info(DWORD handle, std::string& title, std::string& artist)
    {
        const auto* raw = static_cast<const char*>(bass_api::channel_get_tags(handle, bass_api::bass_tag_riff_info));
        if (raw == nullptr) return;

        // RIFF INFO: "INAM=text\0IART=text\0...\0\0" per BASS documentation.
        const char* p = raw;
        const char* const walker_end = p + max_tag_walker_guard; // sanity limit, not real bounds

        // Guard before dereference.
        while (p < walker_end && *p != '\0')
        {
            const char* eq = static_cast<const char*>(std::memchr(p, '=', static_cast<std::size_t>(walker_end - p)));
            if (eq == nullptr) break; // malformed entry, stop

            const auto key_len = static_cast<std::size_t>(eq - p);
            if (key_len == 0) break; // empty key

            std::string key(p, key_len);
            logger::to_upper(key);

            const char* value_start = eq + 1;
            const std::size_t value_max = static_cast<std::size_t>(walker_end - value_start);
            const std::size_t value_len  = bounded_strlen(value_start, value_start + value_max);
            std::string value = fs::ansi_to_utf8(std::string(value_start, value_len));
            strip_trailing(value);
            logger::trim(value);

            if (!value.empty())
            {
                if (key == "INAM" && title == "N/A")
                    title = std::move(value);
                else if (key == "IART" && artist == "N/A")
                    artist = std::move(value);
            }

            if (title != "N/A" && artist != "N/A") return;

            p += key_len + 1 + value_len + 1; // key + '=' + value + '\0'
        }
    }

} // anonymous namespace

void read_metadata(std::uint32_t stream_handle, const std::string& extension,
                   std::string& title, std::string& artist)
{
    const DWORD handle = static_cast<DWORD>(stream_handle);

    // MP1 shares the MPEG/ID3 path for legacy compatibility; it is not a primary QA target.
    if (extension == "mp3" || extension == "mp1" || extension == "mp2" || extension == "aif")
    {
        parse_id3v2(handle, title, artist);
        parse_id3v1(handle, title, artist);
    }
    else if (extension == "ogg")
    {
        parse_vorbis(handle, title, artist);
    }
    else if (extension == "wav")
    {
        parse_riff_info(handle, title, artist);
    }
}
