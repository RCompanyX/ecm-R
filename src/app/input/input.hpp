#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _M_AMD64
std::int64_t ImGui_ImplWin32_WndProcHandler(::HWND, std::uint32_t, std::uint64_t, std::int64_t);
#else
long ImGui_ImplWin32_WndProcHandler(::HWND, std::uint32_t, std::uint32_t, long);
#endif

class input
{
public:
	enum callback_type : std::uint8_t
	{
		on_key_down,
		on_key_up,
	};

	enum result_type : std::uint8_t
	{
		cont,
		interrupt,
	};

	enum class hotkey_action : std::uint8_t
	{
		toggle_overlay,
		pause_track,
		previous_track,
		skip_track,
		toggle_shuffle,
		toggle_repeat,
		count,
	};

	struct hotkey_binding
	{
		hotkey_action action;
		const char* label;
		const char* ini_key;
		std::uint32_t default_key;
		bool starts_unbound;
		bool poll_in_update;
		std::uint32_t* runtime_key;
	};

	using callback = input::result_type(__cdecl*)(std::uint32_t);

	static constexpr std::uint32_t unbound_key = 0;
	static constexpr std::size_t hotkey_count = static_cast<std::size_t>(hotkey_action::count);

	static void init_overlay();
	static void update();
	static std::uint32_t key_from_string(const char* key_text, std::uint32_t default_key);
	static std::string key_to_string(std::uint32_t key);
	static bool is_supported_key(std::uint32_t key);
	static const char* supported_key_help();
	static const std::array<input::hotkey_binding, input::hotkey_count>& hotkey_bindings();
	static bool assign_hotkey(input::hotkey_action action, std::uint32_t key, std::string* error_message = nullptr);
	static void reset_hotkey(input::hotkey_action action);
	static void reset_all_hotkeys();
	static bool begin_hotkey_capture(input::hotkey_action action);
	static void cancel_hotkey_capture();
	static bool is_hotkey_capture_active();
	static input::hotkey_action captured_hotkey_action();
	static input::hotkey_action capture_feedback_action();
	static const char* capture_feedback_message();
	static bool capture_feedback_is_error();
	static void clear_capture_feedback();
	static void on(input::callback_type type, input::callback callback);
	static bool is_key_down(std::uint32_t key);

	static std::unordered_map<input::callback_type, std::vector<input::callback>> callbacks_;
	static std::uint32_t toggle_overlay_key;
	static std::uint32_t pause_track_key;
	static std::uint32_t previous_track_key;
	static std::uint32_t skip_track_key;
	static std::uint32_t toggle_shuffle_key;
	static std::uint32_t toggle_repeat_key;
};
