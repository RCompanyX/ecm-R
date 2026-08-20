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

/// Handles overlay input, hotkey rebinding, and low-level key callbacks.
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
		const char* label_key;
		const char* ini_key;
		std::uint32_t default_key;
		bool starts_unbound;
		bool poll_in_update;
		std::uint32_t* runtime_key;
	};

	using callback = input::result_type(__cdecl*)(std::uint32_t);

	static constexpr std::uint32_t unbound_key = 0;
	static constexpr std::size_t hotkey_count = static_cast<std::size_t>(hotkey_action::count);

	/// Hooks the game window so the overlay and hotkeys can receive input.
	static void init_overlay();
	/// Polls keys, drives hotkey capture, and dispatches registered callbacks.
	static void update();
	/// Parses an INI key name into a virtual-key code, falling back when unknown.
	static std::uint32_t key_from_string(const char* key_text, std::uint32_t default_key);
	/// Converts a virtual-key code into the INI/display name used by the overlay.
	static std::string key_to_string(std::uint32_t key);
	/// Returns whether a key can be bound through the hotkey system.
	static bool is_supported_key(std::uint32_t key);
	/// Returns the short help text shown when binding validation fails.
	static const char* supported_key_help();
	/// Exposes the static hotkey table used by settings and the overlay.
	static const std::array<input::hotkey_binding, input::hotkey_count>& hotkey_bindings();
	/// Assigns a runtime hotkey, rejecting unsupported keys and duplicates.
	static bool assign_hotkey(input::hotkey_action action, std::uint32_t key, std::string* error_message = nullptr);
	/// Restores one hotkey to its default or unbound value.
	static void reset_hotkey(input::hotkey_action action);
	/// Restores every hotkey to its default configuration.
	static void reset_all_hotkeys();
	/// Starts listening for the next pressed key for the requested action.
	static bool begin_hotkey_capture(input::hotkey_action action);
	/// Aborts an in-progress hotkey capture without changing bindings.
	static void cancel_hotkey_capture();
	/// Returns whether the overlay is currently waiting for a new hotkey keypress.
	static bool is_hotkey_capture_active();
	/// Reports which action is currently being rebound.
	static input::hotkey_action captured_hotkey_action();
	/// Returns the action associated with the latest capture feedback message.
	static input::hotkey_action capture_feedback_action();
	/// Returns the latest success or error message from hotkey capture.
	static const char* capture_feedback_message();
	/// Indicates whether the latest capture feedback should be shown as an error.
	static bool capture_feedback_is_error();
	/// Clears the transient hotkey capture feedback shown by the overlay.
	static void clear_capture_feedback();
	/// Registers a callback for low-level key down or key up events.
	static void on(input::callback_type type, input::callback callback);
	/// Reads the current asynchronous state of a virtual key.
	static bool is_key_down(std::uint32_t key);

	static std::unordered_map<input::callback_type, std::vector<input::callback>> callbacks_;
	static std::uint32_t toggle_overlay_key;
	static std::uint32_t pause_track_key;
	static std::uint32_t previous_track_key;
	static std::uint32_t skip_track_key;
	static std::uint32_t toggle_shuffle_key;
	static std::uint32_t toggle_repeat_key;
};
