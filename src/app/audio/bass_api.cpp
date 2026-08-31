#include "bass_api.hpp"

#include "global.hpp"
#include "fs/fs.hpp"
#include "logger/logger.hpp"

#include <atomic>

namespace bass_api
{
    namespace
    {
        using get_version_fn = DWORD(WINAPI*)();
        using init_fn = BOOL(WINAPI*)(int, DWORD, DWORD, HWND, void*);
        using channel_is_active_fn = DWORD(WINAPI*)(DWORD);
        using channel_set_attribute_fn = BOOL(WINAPI*)(DWORD, DWORD, float);
        using stream_free_fn = BOOL(WINAPI*)(DWORD);
        using start_fn = BOOL(WINAPI*)();
        using pause_fn = BOOL(WINAPI*)();
        using set_config_fn = BOOL(WINAPI*)(DWORD, DWORD);
        using stream_create_file_fn = DWORD(WINAPI*)(BOOL, const void*, unsigned long long, unsigned long long, DWORD);
        using channel_play_fn = BOOL(WINAPI*)(DWORD, BOOL);
        using channel_get_tags_fn = const void*(WINAPI*)(DWORD, DWORD);
        using error_get_code_fn = int(WINAPI*)();

        HMODULE module_handle = nullptr;
        get_version_fn get_version_ptr = nullptr;
        init_fn init_ptr = nullptr;
        channel_is_active_fn channel_is_active_ptr = nullptr;
        channel_set_attribute_fn channel_set_attribute_ptr = nullptr;
        stream_free_fn stream_free_ptr = nullptr;
        start_fn start_ptr = nullptr;
        pause_fn pause_ptr = nullptr;
        set_config_fn set_config_ptr = nullptr;
        stream_create_file_fn stream_create_file_ptr = nullptr;
        channel_play_fn channel_play_ptr = nullptr;
        channel_get_tags_fn channel_get_tags_ptr = nullptr;
        error_get_code_fn error_get_code_ptr = nullptr;
        std::atomic_bool device_initialized{ false };
        std::string last_error_message;

        bool can_use_device()
        {
            return device_initialized.load(std::memory_order_acquire) &&
                !global::shutdown.load(std::memory_order_acquire);
        }

        std::string format_system_error(DWORD error)
        {
            LPWSTR buffer = nullptr;
            const DWORD length = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr,
                error,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPWSTR>(&buffer),
                0,
                nullptr);

            if (length == 0 || buffer == nullptr)
            {
                return logger::va("Windows error %lu", error);
            }

            std::wstring wide_message(buffer, length);
            LocalFree(buffer);

            while (!wide_message.empty() && (wide_message.back() == L'\r' || wide_message.back() == L'\n'))
            {
                wide_message.pop_back();
            }

            const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, wide_message.data(), static_cast<int>(wide_message.size()), nullptr, 0, nullptr, nullptr);
            if (utf8_length <= 0)
            {
                return logger::va("Windows error %lu", error);
            }

            std::string message(static_cast<std::size_t>(utf8_length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide_message.data(), static_cast<int>(wide_message.size()), message.data(), utf8_length, nullptr, nullptr);
            return logger::va("Windows error %lu: %s", error, message.c_str());
        }

        std::string get_module_directory()
        {
            char module_path[MAX_PATH]{};
            const DWORD length = GetModuleFileNameA(global::self, module_path, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
            {
                return std::string();
            }

            std::string directory(module_path, length);
            const auto separator = directory.find_last_of("\\/");
            if (separator == std::string::npos)
            {
                return std::string();
            }

            directory.erase(separator);
            return directory;
        }

        void reset()
        {
            get_version_ptr = nullptr;
            init_ptr = nullptr;
            channel_is_active_ptr = nullptr;
            channel_set_attribute_ptr = nullptr;
            stream_free_ptr = nullptr;
            start_ptr = nullptr;
            pause_ptr = nullptr;
            set_config_ptr = nullptr;
            stream_create_file_ptr = nullptr;
            channel_play_ptr = nullptr;
            channel_get_tags_ptr = nullptr;
            error_get_code_ptr = nullptr;
        }

        template <typename T>
        bool resolve(T& target, const char* name)
        {
            target = reinterpret_cast<T>(GetProcAddress(module_handle, name));
            if (target == nullptr)
            {
                last_error_message = logger::va("Missing BASS export '%s'", name);
            }
            return target != nullptr;
        }
    }

    bool load()
    {
        if (global::shutdown.load(std::memory_order_acquire))
        {
            return false;
        }

        if (module_handle != nullptr)
        {
            return true;
        }

        const std::string module_directory = get_module_directory();
        const std::string bass_path = module_directory.empty()
            ? std::string("bass.dll")
            : module_directory + "\\bass.dll";
		logger::log_debug(logger::va("Loading BASS from '%s'", fs::ansi_to_utf8(bass_path).c_str()));

        module_handle = LoadLibraryA(bass_path.c_str());
        if (module_handle == nullptr)
        {
            const DWORD error = GetLastError();
            reset();
            last_error_message = logger::va("%s\nTried path: %s", format_system_error(error).c_str(), fs::ansi_to_utf8(bass_path).c_str());
			logger::log_error(logger::va("BASS load failed: %s", last_error_message.c_str()));
            return false;
        }

        if (!resolve(get_version_ptr, "BASS_GetVersion") ||
            !resolve(init_ptr, "BASS_Init") ||
            !resolve(channel_is_active_ptr, "BASS_ChannelIsActive") ||
            !resolve(channel_set_attribute_ptr, "BASS_ChannelSetAttribute") ||
            !resolve(stream_free_ptr, "BASS_StreamFree") ||
            !resolve(start_ptr, "BASS_Start") ||
            !resolve(pause_ptr, "BASS_Pause") ||
            !resolve(set_config_ptr, "BASS_SetConfig") ||
            !resolve(stream_create_file_ptr, "BASS_StreamCreateFile") ||
            !resolve(channel_play_ptr, "BASS_ChannelPlay") ||
            !resolve(channel_get_tags_ptr, "BASS_ChannelGetTags") ||
            !resolve(error_get_code_ptr, "BASS_ErrorGetCode"))
        {
			logger::log_error(logger::va("BASS export resolution failed: %s", last_error_message.c_str()));
            unload();
            return false;
        }

		logger::log_debug("BASS exports resolved");
        return true;
    }

    void unload()
    {
        device_initialized.store(false, std::memory_order_release);
        reset();
        if (module_handle != nullptr)
        {
            FreeLibrary(module_handle);
            module_handle = nullptr;
        }
    }

    bool is_available()
    {
        return module_handle != nullptr;
    }

    const std::string& last_error()
    {
        return last_error_message;
    }

    int last_call_error()
    {
        return error_get_code_ptr != nullptr ? error_get_code_ptr() : -1;
    }

    DWORD get_version()
    {
        if (global::shutdown.load(std::memory_order_acquire) || get_version_ptr == nullptr)
        {
            return 0;
        }

        return get_version_ptr();
    }

    bool init_device(HWND hwnd)
    {
        device_initialized.store(false, std::memory_order_release);
        if (global::shutdown.load(std::memory_order_acquire) || init_ptr == nullptr)
        {
            return false;
        }

        const bool initialized = init_ptr(-1, 44100, 0, hwnd, nullptr) != FALSE;
        if (initialized && !global::shutdown.load(std::memory_order_acquire))
        {
            device_initialized.store(true, std::memory_order_release);
        }

        return initialized && !global::shutdown.load(std::memory_order_acquire);
    }

    DWORD channel_is_active(DWORD channel)
    {
        if (!can_use_device() || channel_is_active_ptr == nullptr)
        {
            return active_stopped;
        }

        return channel_is_active_ptr(channel);
    }

    bool channel_set_attribute(DWORD channel, DWORD attribute, float value)
    {
        return can_use_device() && channel_set_attribute_ptr != nullptr && channel_set_attribute_ptr(channel, attribute, value) != FALSE;
    }

    bool stream_free(DWORD channel)
    {
        return can_use_device() && stream_free_ptr != nullptr && stream_free_ptr(channel) != FALSE;
    }

    bool start()
    {
        return can_use_device() && start_ptr != nullptr && start_ptr() != FALSE;
    }

    bool pause()
    {
        return can_use_device() && pause_ptr != nullptr && pause_ptr() != FALSE;
    }

    bool set_config(DWORD option, DWORD value)
    {
        return can_use_device() && set_config_ptr != nullptr && set_config_ptr(option, value) != FALSE;
    }

    bool set_channel_volume(DWORD channel, float volume)
    {
        return channel_set_attribute(channel, attrib_vol, volume);
    }

    bool set_stream_volume_config(std::int32_t volume)
    {
        return set_config(config_gvol_stream, static_cast<DWORD>(volume * 100));
    }

    stream_handle_t stream_create_file(const char* file)
    {
        if (!can_use_device() || stream_create_file_ptr == nullptr || file == nullptr)
        {
            return 0;
        }

        const int wide_len = MultiByteToWideChar(CP_UTF8, 0, file, -1, nullptr, 0);
        if (wide_len <= 0)
        {
            return 0;
        }

        std::wstring wfile(static_cast<size_t>(wide_len), L'\0');
        if (MultiByteToWideChar(CP_UTF8, 0, file, -1, wfile.data(), wide_len) != wide_len)
        {
            return 0;
        }

        return stream_create_file_ptr(FALSE, wfile.c_str(), 0, 0, sample_float | bass_unicode);
    }

    bool channel_play(DWORD channel, bool restart)
    {
        return can_use_device() && channel_play_ptr != nullptr && channel_play_ptr(channel, restart ? TRUE : FALSE) != FALSE;
    }

    const void* channel_get_tags(stream_handle_t handle, DWORD tag_type)
    {
        if (!can_use_device() || channel_get_tags_ptr == nullptr) return nullptr;
        return channel_get_tags_ptr(handle, tag_type);
    }
}
