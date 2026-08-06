#pragma once

/// Legacy stream player entry point used by the audio subsystem.
void play_file(const char* file, int channel);

/// Resolves embedded title/artist metadata and applies the filename fallback.
void resolve_file_metadata(const char* file, std::uint32_t stream_handle,
                           std::string& title, std::string& artist);
