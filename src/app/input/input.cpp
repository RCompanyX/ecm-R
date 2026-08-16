#include "global.hpp"

#include "logger/logger.hpp"
#include "audio/audio.hpp"
#include "settings/settings.hpp"
#include "localization/localization.hpp"

#include "input.hpp"

#include <array>

WNDPROC o_wndproc;
bool toggle_once = false;
bool pause_once = false;
bool previous_once = false;
bool skip_once = false;
bool shuffle_once = false;
bool repeat_once = false;
std::unordered_map<input::callback_type, std::vector<input::callback>> input::callbacks_;

namespace
{
	using hotkey_action = input::hotkey_action;

	// Tracks an in-progress hotkey rebind and the feedback shown by the overlay.
	struct hotkey_capture_state_t
	{
		bool active = false;
		hotkey_action action = hotkey_action::count;
		hotkey_action feedback_action = hotkey_action::count;
		std::string message;
		bool is_error = false;
	};

	hotkey_capture_state_t hotkey_capture_state;

	// Finds the static binding descriptor for a hotkey action.
	const input::hotkey_binding* find_hotkey_binding(const hotkey_action action)
	{
		for (const auto& binding : input::hotkey_bindings())
		{
			if (binding.action == action)
			{
				return &binding;
			}
		}

		return nullptr;
	}

	// Returns the one-shot latch used to debounce a specific hotkey action.
	bool* latch_for_action(const hotkey_action action)
	{
		switch (action)
		{
		case hotkey_action::toggle_overlay:
			return &toggle_once;

		case hotkey_action::pause_track:
			return &pause_once;

		case hotkey_action::previous_track:
			return &previous_once;

		case hotkey_action::skip_track:
			return &skip_once;

		case hotkey_action::toggle_shuffle:
			return &shuffle_once;

		case hotkey_action::toggle_repeat:
			return &repeat_once;

		default:
			return nullptr;
		}
	}

	// Mirrors the audio subsystem lock that suppresses playback hotkeys during unsafe states.
	bool hotkeys_locked()
	{
		return audio::are_hotkeys_locked();
	}

	// Clears the transient capture result shown after a rebind attempt.
	void clear_capture_feedback_state()
	{
		hotkey_capture_state.feedback_action = hotkey_action::count;
		hotkey_capture_state.message.clear();
		hotkey_capture_state.is_error = false;
	}

	// Stores the latest success or failure message for the active hotkey action.
	void set_capture_feedback(const hotkey_action action, const std::string& message, const bool is_error)
	{
		hotkey_capture_state.feedback_action = action;
		hotkey_capture_state.message = message;
		hotkey_capture_state.is_error = is_error;
	}

	// Refreshes every hotkey latch from the current keyboard state to avoid stale presses.
	void sync_hotkey_latches()
	{
		for (const auto& binding : input::hotkey_bindings())
		{
			bool* latch = latch_for_action(binding.action);
			if (!latch)
			{
				continue;
			}

			const std::uint32_t key = *binding.runtime_key;
			*latch = key != input::unbound_key && (GetAsyncKeyState(key) & 0x8000) != 0;
		}
	}

	// Small wrapper used by the hotkey dispatcher for next-track navigation.
	void skip_to_next_track()
	{
		audio::skip_to_next_track();
	}

	// Small wrapper used by the hotkey dispatcher for previous-track navigation.
	void skip_to_previous_track()
	{
		audio::play_previous_song();
	}

	// Small wrapper used by the hotkey dispatcher for manual pause toggling.
	void toggle_manual_pause()
	{
		audio::toggle_manual_pause();
	}

	// Small wrapper used by the hotkey dispatcher for shuffle toggling.
	void toggle_shuffle_enabled()
	{
		audio::toggle_shuffle_enabled();
	}

	// Small wrapper used by the hotkey dispatcher for repeat toggling.
	void toggle_repeat_enabled()
	{
		audio::toggle_repeat_enabled();
	}

	// Executes the runtime action associated with a hotkey binding.
	void execute_hotkey_action(const hotkey_action action)
	{
		switch (action)
		{
		case hotkey_action::toggle_overlay:
			global::hide = !global::hide;
			break;

		case hotkey_action::pause_track:
			toggle_manual_pause();
			break;

		case hotkey_action::previous_track:
			skip_to_previous_track();
			break;

		case hotkey_action::skip_track:
			skip_to_next_track();
			break;

		case hotkey_action::toggle_shuffle:
			toggle_shuffle_enabled();
			break;

		case hotkey_action::toggle_repeat:
			toggle_repeat_enabled();
			break;

		default:
			break;
		}
	}

	// Matches a pressed key against the configured bindings and debounces repeated triggers.
	bool dispatch_hotkey(const std::uint32_t key)
	{
		if (key == input::unbound_key)
		{
			return false;
		}

		for (const auto& binding : input::hotkey_bindings())
		{
			if (*binding.runtime_key != key)
			{
				continue;
			}

			bool* latch = latch_for_action(binding.action);
			if (latch && *latch)
			{
				return true;
			}

			execute_hotkey_action(binding.action);
			if (latch)
			{
				*latch = true;
			}

			return true;
		}

		return false;
	}

	// Releases the latch for a hotkey once its key is no longer pressed.
	void release_hotkey(const std::uint32_t key)
	{
		if (key == input::unbound_key)
		{
			return;
		}

		for (const auto& binding : input::hotkey_bindings())
		{
			if (*binding.runtime_key != key)
			{
				continue;
			}

			if (bool* latch = latch_for_action(binding.action))
			{
				*latch = false;
			}

			return;
		}
	}

	// Applies a captured key to the active rebind request and persists it when possible.
	bool try_capture_hotkey(const std::uint32_t key)
	{
		if (!hotkey_capture_state.active)
		{
			return false;
		}

		if (!input::is_supported_key(key))
		{
			set_capture_feedback(hotkey_capture_state.action, localization::format("input.unsupported_key", {{ "keys", input::supported_key_help() }}), true);
			return true;
		}

		std::string error_message;
		const input::hotkey_binding* binding = find_hotkey_binding(hotkey_capture_state.action);
		const std::uint32_t previous_key = binding ? *binding->runtime_key : input::unbound_key;
		if (!input::assign_hotkey(hotkey_capture_state.action, key, &error_message))
		{
			set_capture_feedback(hotkey_capture_state.action, error_message, true);
			return true;
		}

		const hotkey_action action = hotkey_capture_state.action;
		hotkey_capture_state.active = false;
		hotkey_capture_state.action = hotkey_action::count;
		const std::string key_name = input::key_to_string(key);
		if (binding && !settings::save_hotkey_binding(binding->ini_key, key))
		{
			input::assign_hotkey(action, previous_key);
			set_capture_feedback(action, localization::format("input.save_failed", {{ "key", key_name }}), true);
		}
		else
		{
			set_capture_feedback(action, localization::format("input.bound_to", {{ "key", key_name }}), false);
		}
		sync_hotkey_latches();
		return true;
	}
}

// Returns the canonical binding table used by input, menus, and settings.
const std::array<input::hotkey_binding, input::hotkey_count>& input::hotkey_bindings()
{
	static const std::array<input::hotkey_binding, input::hotkey_count> bindings = {{
		{ hotkey_action::toggle_overlay, "hotkey.toggle_overlay", "toggle_overlay", VK_F11, false, false, &input::toggle_overlay_key },
		{ hotkey_action::pause_track, "hotkey.pause_track", "pause_track", VK_F8, false, true, &input::pause_track_key },
		{ hotkey_action::previous_track, "hotkey.previous_track", "previous_track", VK_F9, false, true, &input::previous_track_key },
		{ hotkey_action::skip_track, "hotkey.skip_track", "skip_track", VK_F10, false, true, &input::skip_track_key },
		{ hotkey_action::toggle_shuffle, "hotkey.toggle_shuffle", "toggle_shuffle", input::unbound_key, true, true, &input::toggle_shuffle_key },
		{ hotkey_action::toggle_repeat, "hotkey.toggle_repeat", "toggle_repeat", input::unbound_key, true, true, &input::toggle_repeat_key },
	}};

	return bindings;
}

// Parses a persisted key name into the corresponding virtual-key code.
std::uint32_t input::key_from_string(const char* key_text, std::uint32_t default_key)
{
	if (!key_text || !key_text[0])
	{
		return default_key;
	}

	if (!_stricmp(key_text, "NONE") || !_stricmp(key_text, "UNBOUND"))
	{
		return input::unbound_key;
	}

	if ((key_text[0] == 'F' || key_text[0] == 'f') && key_text[1] != '\0')
	{
		int function_key = std::atoi(key_text + 1);
		if (function_key >= 1 && function_key <= 24)
		{
			return VK_F1 + function_key - 1;
		}
	}

	if (key_text[1] == '\0')
	{
		if (key_text[0] >= '0' && key_text[0] <= '9')
		{
			return key_text[0];
		}

		if (key_text[0] >= 'A' && key_text[0] <= 'Z')
		{
			return key_text[0];
		}

		if (key_text[0] >= 'a' && key_text[0] <= 'z')
		{
			return key_text[0] - 32;
		}
	}

	if (!_stricmp(key_text, "SPACE")) return VK_SPACE;
	if (!_stricmp(key_text, "TAB")) return VK_TAB;
	if (!_stricmp(key_text, "ENTER")) return VK_RETURN;
	if (!_stricmp(key_text, "ESC") || !_stricmp(key_text, "ESCAPE")) return VK_ESCAPE;
	if (!_stricmp(key_text, "BACKSPACE")) return VK_BACK;
	if (!_stricmp(key_text, "INSERT")) return VK_INSERT;
	if (!_stricmp(key_text, "DELETE")) return VK_DELETE;
	if (!_stricmp(key_text, "HOME")) return VK_HOME;
	if (!_stricmp(key_text, "END")) return VK_END;
	if (!_stricmp(key_text, "PAGEUP")) return VK_PRIOR;
	if (!_stricmp(key_text, "PAGEDOWN")) return VK_NEXT;
	if (!_stricmp(key_text, "UP")) return VK_UP;
	if (!_stricmp(key_text, "DOWN")) return VK_DOWN;
	if (!_stricmp(key_text, "LEFT")) return VK_LEFT;
	if (!_stricmp(key_text, "RIGHT")) return VK_RIGHT;

	return default_key;
}

// Converts a runtime virtual-key code into the display and INI text representation.
std::string input::key_to_string(const std::uint32_t key)
{
	if (key == input::unbound_key)
	{
		return "None";
	}

	if (key >= VK_F1 && key <= VK_F24)
	{
		return std::string("F") + std::to_string((key - VK_F1) + 1);
	}

	if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z'))
	{
		return std::string(1, static_cast<char>(key));
	}

	switch (key)
	{
	case VK_SPACE:
		return "Space";

	case VK_TAB:
		return "Tab";

	case VK_RETURN:
		return "Enter";

	case VK_ESCAPE:
		return "Esc";

	case VK_BACK:
		return "Backspace";

	case VK_INSERT:
		return "Insert";

	case VK_DELETE:
		return "Delete";

	case VK_HOME:
		return "Home";

	case VK_END:
		return "End";

	case VK_PRIOR:
		return "PageUp";

	case VK_NEXT:
		return "PageDown";

	case VK_UP:
		return "Up";

	case VK_DOWN:
		return "Down";

	case VK_LEFT:
		return "Left";

	case VK_RIGHT:
		return "Right";

	default:
		return "Unknown";
	}
}

// Validates whether a virtual-key code is supported by ECM-R hotkey rebinding.
bool input::is_supported_key(const std::uint32_t key)
{
	if (key >= VK_F1 && key <= VK_F24)
	{
		return true;
	}

	if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z'))
	{
		return true;
	}

	switch (key)
	{
	case VK_SPACE:
	case VK_TAB:
	case VK_RETURN:
	case VK_ESCAPE:
	case VK_BACK:
	case VK_INSERT:
	case VK_DELETE:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_UP:
	case VK_DOWN:
	case VK_LEFT:
	case VK_RIGHT:
		return true;

	default:
		return false;
	}
}

// Returns the supported-key list shown in the hotkeys menu and error messages.
const char* input::supported_key_help()
{
	return localization::text("hotkeys.supported_keys");
}

// Assigns a hotkey after validating support and checking for duplicate bindings.
bool input::assign_hotkey(const input::hotkey_action action, const std::uint32_t key, std::string* error_message)
{
	if (error_message)
	{
		error_message->clear();
	}

	const hotkey_binding* binding = find_hotkey_binding(action);
	if (!binding)
	{
		if (error_message)
		{
			*error_message = localization::text("input.unknown_action");
		}

		return false;
	}

	if (key != input::unbound_key && !input::is_supported_key(key))
	{
		if (error_message)
		{
			*error_message = localization::format("input.unsupported_key", {{ "keys", input::supported_key_help() }});
		}

		return false;
	}

	if (key != input::unbound_key)
	{
		for (const auto& other_binding : input::hotkey_bindings())
		{
			if (other_binding.action == action)
			{
				continue;
			}

			if (*other_binding.runtime_key != key)
			{
				continue;
			}

			if (error_message)
			{
				*error_message = localization::format("input.duplicate", {{ "key", input::key_to_string(key) }, { "action", localization::text(other_binding.label_key) }});
			}

			return false;
		}
	}

	*binding->runtime_key = key;
	if (bool* latch = latch_for_action(action))
	{
		*latch = key != input::unbound_key && (GetAsyncKeyState(key) & 0x8000) != 0;
	}

	return true;
}

// Restores one action to its default configured hotkey.
void input::reset_hotkey(const input::hotkey_action action)
{
	if (const hotkey_binding* binding = find_hotkey_binding(action))
	{
		input::assign_hotkey(action, binding->default_key);
	}
}

// Restores every action to its default key without touching persistence.
void input::reset_all_hotkeys()
{
	for (const auto& binding : input::hotkey_bindings())
	{
		*binding.runtime_key = binding.default_key;
	}

	sync_hotkey_latches();
}

// Starts capturing the next pressed key for a specific action.
bool input::begin_hotkey_capture(const input::hotkey_action action)
{
	if (!find_hotkey_binding(action))
	{
		return false;
	}

	hotkey_capture_state.active = true;
	hotkey_capture_state.action = action;
	clear_capture_feedback_state();
	sync_hotkey_latches();
	return true;
}

// Cancels the active capture and reports that it was intentionally aborted.
void input::cancel_hotkey_capture()
{
	if (!hotkey_capture_state.active)
	{
		return;
	}

	const hotkey_action action = hotkey_capture_state.action;
	hotkey_capture_state.active = false;
	hotkey_capture_state.action = hotkey_action::count;
	set_capture_feedback(action, localization::text("input.canceled"), false);
	sync_hotkey_latches();
}

// Reports whether the input system is currently waiting for a replacement key.
bool input::is_hotkey_capture_active()
{
	return hotkey_capture_state.active;
}

// Returns the action currently being rebound, if any.
input::hotkey_action input::captured_hotkey_action()
{
	return hotkey_capture_state.active ? hotkey_capture_state.action : hotkey_action::count;
}

// Returns which action owns the latest capture feedback message.
input::hotkey_action input::capture_feedback_action()
{
	return hotkey_capture_state.feedback_action;
}

// Returns the latest hotkey capture feedback message.
const char* input::capture_feedback_message()
{
	return hotkey_capture_state.message.c_str();
}

// Tells the overlay whether the latest capture feedback is an error or a success.
bool input::capture_feedback_is_error()
{
	return hotkey_capture_state.is_error;
}

// Clears any capture feedback that is still visible in the overlay.
void input::clear_capture_feedback()
{
	clear_capture_feedback_state();
}

// Window procedure hook that forwards keyboard input to ImGui and ECM-R callbacks.
LRESULT __stdcall wndproc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

	switch (uMsg)
	{

	case WM_SYSKEYDOWN:
		if (wParam == VK_RETURN)
		{
			if ((HIWORD(lParam) & KF_ALTDOWN))
			{
				return 1;
			}
		}
		break;

	case WM_KEYDOWN:
		for (const auto callback : input::callbacks_[input::callback_type::on_key_down])
		{
			callback(wParam);
		}
		break;

	case WM_KEYUP:
		for (const auto callback : input::callbacks_[input::callback_type::on_key_up])
		{
			callback(wParam);
		}
		break;

	case WM_SYSCOMMAND:
		if ((wParam & 0xFF00) == SC_KEYMENU)
		{
			return 1;
		}
	}


	return CallWindowProcA(o_wndproc, hWnd, uMsg, wParam, lParam);
}

// Installs the window hook and registers the key down and key up handlers used by ECM-R.
void input::init_overlay()
{
	o_wndproc = (WNDPROC)SetWindowLongW(global::hwnd, GWLP_WNDPROC, (LONG_PTR)wndproc);

	input::on(input::callback_type::on_key_down, [](auto key) -> input::result_type
	{
		if (input::is_hotkey_capture_active())
		{
			try_capture_hotkey(key);
			return input::result_type::cont;
		}

		if (hotkeys_locked())
		{
			return input::result_type::cont;
		}

		dispatch_hotkey(key);

		return input::result_type::cont;
	});

	input::on(input::callback_type::on_key_up, [](auto key) -> input::result_type
	{
		release_hotkey(key);

		return input::result_type::cont;
	});
}


// Polls update-driven hotkeys and keeps their latches in sync across frames.
void input::update()
{
	if (hotkeys_locked() || input::is_hotkey_capture_active())
	{
		sync_hotkey_latches();
		return;
	}

	for (const auto& binding : input::hotkey_bindings())
	{
		if (!binding.poll_in_update)
		{
			continue;
		}

		bool* latch = latch_for_action(binding.action);
		if (!latch)
		{
			continue;
		}

		const std::uint32_t key = *binding.runtime_key;
		if (key == input::unbound_key)
		{
			*latch = false;
			continue;
		}

		const bool key_down = (GetAsyncKeyState(key) & 0x8000) != 0;
		if (key_down && !*latch)
		{
			execute_hotkey_action(binding.action);
			*latch = true;
		}
		else if (!key_down && *latch)
		{
			*latch = false;
		}
	}
}


// Registers a callback for one of the low-level keyboard event channels.
void input::on(input::callback_type type, input::callback callback)
{
	input::callbacks_[type].emplace_back(callback);
}

// Queries whether an ImGui key was pressed during the current frame.
bool input::is_key_down(std::uint32_t key)
{
	return ImGui::IsKeyPressed(key, false);
}

std::uint32_t input::toggle_overlay_key = VK_F11;
std::uint32_t input::pause_track_key = VK_F8;
std::uint32_t input::previous_track_key = VK_F9;
std::uint32_t input::skip_track_key = VK_F10;
std::uint32_t input::toggle_shuffle_key = input::unbound_key;
std::uint32_t input::toggle_repeat_key = input::unbound_key;
