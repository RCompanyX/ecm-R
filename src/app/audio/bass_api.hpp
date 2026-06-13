#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>

/// Thin wrapper around the dynamically loaded BASS runtime used by ECM-R.
namespace bass_api
{
    using stream_handle_t = DWORD;

    inline constexpr DWORD version = 0x204;
    inline constexpr DWORD active_stopped = 0;
    inline constexpr DWORD attrib_vol = 2;
    // Unused: float 32-bit decode doubles memory/CPU during stream creation.
    // inline constexpr DWORD sample_float = 0x100;
    inline constexpr DWORD stream_prescan = 0x20000;
    inline constexpr DWORD config_buffer = 0;
    inline constexpr DWORD config_gvol_stream = 5;
    inline constexpr DWORD bass_unicode = 0x80000000;

    /// Loads bass.dll from the plugin directory and resolves the required exports.
    bool load();
    /// Releases bass.dll and clears the resolved function pointers.
    void unload();
    /// Returns whether bass.dll was loaded successfully for this session.
    bool is_available();
    /// Returns the latest human-readable BASS loading or call error.
    const std::string& last_error();
    /// Returns the loaded BASS version so compatibility can be verified.
    DWORD get_version();
    /// Initializes the default BASS output device against the game window.
    bool init_device(HWND hwnd);
    /// Mirrors BASS_ChannelIsActive for ECM-R playback checks.
    DWORD channel_is_active(DWORD channel);
    /// Sets a raw BASS channel attribute on the active stream.
    bool channel_set_attribute(DWORD channel, DWORD attribute, float value);
    /// Frees a stream handle created for a playlist file.
    bool stream_free(DWORD channel);
    /// Starts or resumes the currently active BASS output.
    bool start();
    /// Pauses the currently active BASS output.
    bool pause();
    /// Applies a raw BASS global configuration value.
    bool set_config(DWORD option, DWORD value);
    /// Sets the volume of a single BASS channel using a 0.0-1.0 range.
    bool set_channel_volume(DWORD channel, float volume);
    /// Applies the master stream volume using ECM-R's 0-100 range.
    bool set_stream_volume_config(std::int32_t volume);
    /// Opens a file-backed BASS stream for a playlist entry (UTF-8 path).
    stream_handle_t stream_create_file(const char* file);
    /// Starts playback on a stream handle, optionally from the beginning.
    bool channel_play(DWORD channel, bool restart);
}
