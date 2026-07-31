#include "audio.hpp"
#include "bass_api.hpp"
#include "logger/logger.hpp"
#include "fs/fs.hpp"

#include <algorithm>
#include <cstring>

namespace
{
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
    std::string utf16_to_utf8(const char* data, std::size_t len, bool little_endian)
    {
        if (len < 2) return {};

        const auto* src = reinterpret_cast<const std::uint16_t*>(data);
        const std::size_t num_units = len / 2;

        // Build a wide string, swapping bytes if needed.
        std::wstring wide;
        wide.reserve(num_units);
        for (std::size_t i = 0; i < num_units; ++i)
        {
            std::uint16_t unit = src[i];
            if (!little_endian)
                unit = static_cast<std::uint16_t>((unit << 8) | (unit >> 8));
            if (unit == 0) break; // null termination
            wide.push_back(static_cast<wchar_t>(unit));
        }

        return fs::wstring_to_utf8(wide);
    }

    // --- parsers ---

    void parse_id3v2(DWORD handle, std::string& title, std::string& artist)
    {
        const auto* raw = static_cast<const char*>(bass_api::channel_get_tags(handle, bass_api::bass_tag_id3v2));
        if (raw == nullptr) return;

        // We need the tag size. BASS returns the raw ID3v2 data but the tag size
        // must be derived from the header. Minimum ID3v2 header is 10 bytes.
        // BASS docs don't specify exactly how much data is returned, so we use
        // the declared tag size from the header to bound pointer arithmetic.

        // First 3 bytes: "ID3"
        if (std::memcmp(raw, "ID3", 3) != 0) return;

        const std::uint8_t version_major = safe_byte(raw, 10, 3);
        const std::uint8_t flags          = safe_byte(raw, 10, 5);

        // We only handle v2.3 and v2.4.
        if (version_major != 3 && version_major != 4) return;

        // Tag size: v2.4 is syncsafe; v2.3 is big-endian.
        const std::uint32_t tag_size = (version_major == 4)
            ? syncsafe_u32(raw, 10, 6)
            : be_u32(raw, 10, 6);

        // ponytail: we trust BASS returns at least header+tag_size bytes; if the
        // stream data is truncated by BASS, our bounds checks on read will catch
        // any overflow via safe_byte/be_u32 returning 0.
        const std::size_t total = static_cast<std::size_t>(10) + tag_size;

        // Extended header
        std::size_t cursor = 10;
        if ((flags & 0x40) != 0)
        {
            if (cursor + 4 > total) return;
            const std::uint32_t ext_size = (version_major == 4)
                ? syncsafe_u32(raw, total, cursor)  // size itself is syncsafe
                : be_u32(raw, total, cursor);
            cursor += 4 + ext_size;
            if (cursor >= total) return;
        }

        // ponytail: O(n) frame scan, single-pass; if ID3v2 gets massive (>1 MB)
        // add an early-exit byte budget.
        while (cursor + 10 <= total)
        {
            const char id[5] = { raw[cursor], raw[cursor + 1], raw[cursor + 2], raw[cursor + 3], '\0' };

            // Padding byte or end
            if (id[0] == '\0') break;

            std::uint32_t frame_size = (version_major == 4)
                ? syncsafe_u32(raw, total, cursor + 4)
                : be_u32(raw, total, cursor + 4);

            // v2.3 doesn't use syncsafe for frame sizes, ok.
            // Skip flags
            cursor += 10;

            if (cursor + frame_size > total) break;

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

    void parse_id3v1(DWORD handle, std::string& title, std::string& artist)
    {
        const auto* raw = static_cast<const char*>(bass_api::channel_get_tags(handle, bass_api::bass_tag_id3));
        if (raw == nullptr) return;

        // 128 bytes starting with "TAG"
        // Guard with isprint check on first byte in case BASS returns garbage
        // std::memcmp(raw, "TAG", 3) — but we can't bounds-check length from BASS;
        // BASS returns exactly 128 bytes or nullptr. Trust it.

        if (std::memcmp(raw, "TAG", 3) != 0) return;

        // Title at bytes 3-32 (30 bytes), artist at bytes 33-62 (30 bytes)
        auto read_field = [&](std::size_t off, std::size_t len, std::string& target)
        {
            if (target != "N/A") return; // don't overwrite a better value
            std::string value;
            latin1_to_utf8(raw + off, len, value);
            strip_trailing(value);
            logger::trim(value);
            if (!value.empty())
                target = std::move(value);
        };

        read_field(3, 30, title);
        read_field(33, 30, artist);
    }

    void parse_vorbis(DWORD handle, std::string& title, std::string& artist)
    {
        const auto* raw = static_cast<const char*>(bass_api::channel_get_tags(handle, bass_api::bass_tag_ogg));
        if (raw == nullptr) return;

        // Vorbis comments: "KEY=VALUE\0KEY=VALUE\0...\0\0"
        const char* p = raw;
        const char* const end = p + 0x100000; // ponytail: 1 MB upper bound guard
        while (*p != '\0' && p < end)
        {
            const char* eq = std::strchr(p, '=');
            if (eq == nullptr)
            {
                p += std::strlen(p) + 1;
                continue;
            }

            const auto key_len = static_cast<std::size_t>(eq - p);
            std::string key(p, key_len);
            logger::to_upper(key);

            const char* value_start = eq + 1;
            std::string value(value_start);
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

            p += std::strlen(p) + 1;
        }
    }

    void parse_riff_info(DWORD handle, std::string& title, std::string& artist)
    {
        const auto* raw = static_cast<const char*>(bass_api::channel_get_tags(handle, bass_api::bass_tag_riff_info));
        if (raw == nullptr) return;

        // RIFF INFO: "key\0value\0...key\0value\0\0"
        const char* p = raw;
        const char* const end = p + 0x100000; // ponytail: 1 MB upper bound guard
        while (*p != '\0' && p < end)
        {
            std::string key(p);
            p += key.size() + 1;

            if (*p == '\0') break; // empty value terminates

            std::string ansi_value(p);
            std::string value = fs::ansi_to_utf8(ansi_value);
            strip_trailing(value);
            logger::trim(value);
            logger::to_upper(key);

            if (!value.empty())
            {
                if (key == "INAM" && title == "N/A")
                    title = std::move(value);
                else if (key == "IART" && artist == "N/A")
                    artist = std::move(value);
            }

            if (title != "N/A" && artist != "N/A") return;

            p += ansi_value.size() + 1;
        }
    }

} // anonymous namespace

void read_metadata(std::uint32_t stream_handle, const std::string& extension,
                   std::string& title, std::string& artist)
{
    const DWORD handle = static_cast<DWORD>(stream_handle);

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
