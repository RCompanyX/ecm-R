#include "global.hpp"
#include "fs/fs.hpp"

#include "localization.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <unordered_map>

namespace localization
{
	namespace
	{
		using bundle = std::unordered_map<std::string, std::string>;

		struct compiled_translation
		{
			const char* key;
			const char* value;
		};

		// ponytail: flat key/value bundles keep runtime lookup cheap; sections are unnecessary for this fixed overlay surface.
		constexpr compiled_translation compiled_english[] = {
			{ "menu.actions", "Actions" },
			{ "menu.hotkeys", "Hotkeys" },
			{ "menu.playlist", "Playlist" },
			{ "menu.about", "About" },
			{ "menu.language", "Language" },
			{ "status.listening", "Listening: {track} on {playlist}" },
			{ "status.paused", "Paused" },
			{ "status.playing", "Playing" },
			{ "release.new_stable", "New stable release available" },
			{ "release.new_testing", "New testing pre-release available" },
			{ "release.latest_stable", "Latest stable release" },
			{ "release.latest_testing", "Latest testing pre-release" },
			{ "actions.audio_controls", "Audio Controls" },
			{ "actions.current_volume_ingame", "Current Volume (In-game)" },
			{ "actions.frontend_volume", "Frontend Volume" },
			{ "actions.current_volume_frontend", "Current Volume (Frontend)" },
			{ "actions.ingame_volume", "In-game Volume" },
			{ "actions.resume", "Resume" },
			{ "actions.pause", "Pause" },
			{ "actions.previous", "Previous" },
			{ "actions.skip", "Skip" },
			{ "actions.shuffle", "Shuffle" },
			{ "actions.repeat", "Repeat" },
			{ "actions.ingame_movie_muting", "In-Game Movie Muting" },
			{ "actions.mode", "Mode: {mode}" },
			{ "actions.random", "Random" },
			{ "actions.sequential", "Sequential" },
			{ "actions.repeat_status", "Repeat: {repeat}" },
			{ "actions.all", "All" },
			{ "actions.off", "Off" },
			{ "actions.manual_pause", "Manual Pause: {state}" },
			{ "actions.on", "On" },
			{ "actions.context", "Context: {context}" },
			{ "actions.active_volume", "Active Volume: {volume}" },
			{ "actions.tracks", "Tracks: {count}" },
			{ "context.frontend", "Frontend" },
			{ "context.ingame", "In-game" },
			{ "context.all", "All" },
			{ "hotkeys.locked", "ECM-R hotkeys stay locked until the first startup chyron has appeared and disappeared once." },
			{ "hotkeys.ready", "ECM-R hotkeys are ready. Supported keys: {keys}." },
			{ "hotkeys.supported_keys", "F1-F24, A-Z, 0-9, Space, Tab, Enter, Esc, Backspace, Insert, Delete, Home, End, PageUp, PageDown, Up, Down, Left, Right" },
			{ "hotkeys.capture_suspended", "While capture is active, ECM-R suspends hotkey execution so the candidate key does not trigger playback, overlay, shuffle, or repeat actions." },
			{ "hotkeys.capturing", "Capturing binding for: {action}" },
			{ "hotkeys.press_supported", "Press a supported key to bind this action." },
			{ "hotkeys.cancel", "Cancel" },
			{ "hotkeys.binding_cleared", "Binding cleared." },
			{ "hotkeys.reset_to", "Reset to {key}." },
			{ "hotkeys.capture_progress", "Capture in progress..." },
			{ "hotkeys.overlay_unbound", "Overlay is currently unbound. Rebind it before closing this menu." },
			{ "hotkeys.rebind", "Rebind" },
			{ "hotkeys.clear", "Clear" },
			{ "hotkeys.reset", "Reset" },
			{ "hotkeys.reset_all", "Reset All" },
			{ "hotkeys.all_reset", "All hotkeys reset to their defaults." },
			{ "hotkeys.finish_capture", "Finish or cancel the active capture before resetting all bindings." },
			{ "hotkeys.shuffle_repeat_none", "Shuffle and Repeat start as None by default." },
			{ "hotkeys.save_failed", "Failed to save the hotkey in the INI file." },
			{ "hotkey.toggle_overlay", "Toggle Overlay" },
			{ "hotkey.pause_track", "Pause / Resume" },
			{ "hotkey.previous_track", "Previous Track" },
			{ "hotkey.skip_track", "Skip Track" },
			{ "hotkey.toggle_shuffle", "Toggle Shuffle" },
			{ "hotkey.toggle_repeat", "Toggle Repeat" },
			{ "about.title", "ECM-R - External Custom Music Reloaded" },
			{ "about.version", "Version: {version}" },
			{ "about.fork", "Fork of the original ECM (External Custom Music) project." },
			{ "about.original_author", "Original author: BttrDrgn" },
			{ "about.maintainer", "Current fork maintainer: RCompanyX" },
			{ "about.support", "Report bugs, request features, or share ideas through GitHub Issues." },
			{ "about.repository", "Repository" },
			{ "about.issues", "Issues" },
			{ "about.open_failed", "Failed to open link:\n{url}" },
			{ "language.english", "English" },
			{ "language.spanish", "Español" },
			{ "language.save_failed", "Failed to save the language in the INI file. The current language was kept." },
			{ "input.unsupported_key", "Unsupported key. Use {keys}." },
			{ "input.save_failed", "Failed to save the binding for {key}. The previous key was restored." },
			{ "input.bound_to", "Bound to {key}." },
			{ "input.canceled", "Rebind canceled." },
			{ "input.unknown_action", "Unknown hotkey action." },
			{ "input.duplicate", "{key} is already assigned to {action}." },
			{ "bass.title", "ECM-R BASS" },
			{ "bass.load_failed", "Failed to load bass.dll.\n{detail}\nMake sure bass.dll is in the scripts folder next to ecm-r.x86.asi.\nSee the mod README for where to get bass.dll and where to place it.\nECM-R music will be disabled for this session." },
			{ "bass.wrong_version", "An incorrect version of BASS.DLL was loaded.\nLoaded version: {loaded}.\nExpected version family: {expected}.\nECM-R music will be disabled for this session." },
			{ "bass.device_failed", "Can't initialize the BASS device.\nBASS error: {error}.\nNo audio will play for this session." },
		};

		bundle cached_english;
		bundle cached_spanish;
		language active_language = language::en;
		bool bundles_loaded = false;

		std::string trim_copy(std::string value)
		{
			auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
			auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
			return first < last ? std::string(first, last) : std::string();
		}

		bool valid_utf8(const std::string& value)
		{
			for (std::size_t i = 0; i < value.size(); ++i)
			{
				const unsigned char first = static_cast<unsigned char>(value[i]);
				std::size_t continuation_count = 0;
				std::uint32_t code_point = 0;

				if (first <= 0x7F)
				{
					continue;
				}
				if (first >= 0xC2 && first <= 0xDF)
				{
					continuation_count = 1;
					code_point = first & 0x1F;
				}
				else if (first >= 0xE0 && first <= 0xEF)
				{
					continuation_count = 2;
					code_point = first & 0x0F;
				}
				else if (first >= 0xF0 && first <= 0xF4)
				{
					continuation_count = 3;
					code_point = first & 0x07;
				}
				else
				{
					return false;
				}

				if (i + continuation_count >= value.size())
				{
					return false;
				}

				for (std::size_t j = 1; j <= continuation_count; ++j)
				{
					const unsigned char next = static_cast<unsigned char>(value[i + j]);
					if ((next & 0xC0) != 0x80)
					{
						return false;
					}
					code_point = (code_point << 6) | (next & 0x3F);
				}

				if ((continuation_count == 2 && code_point < 0x800) ||
					(continuation_count == 3 && code_point < 0x10000) ||
					(code_point >= 0xD800 && code_point <= 0xDFFF) || code_point > 0x10FFFF)
				{
					return false;
				}

				i += continuation_count;
			}

			return true;
		}

		bool valid_placeholder_name(const std::string& name)
		{
			if (name.empty())
			{
				return false;
			}

			return std::all_of(name.begin(), name.end(), [](unsigned char ch) {
				return std::isalnum(ch) != 0 || ch == '_' || ch == '-' || ch == '.';
			});
		}

		bool collect_placeholders(const std::string& value, std::unordered_map<std::string, int>& placeholders)
		{
			for (std::size_t i = 0; i < value.size(); ++i)
			{
				if (value[i] == '}')
				{
					return false;
				}
				if (value[i] != '{')
				{
					continue;
				}

				const std::size_t end = value.find('}', i + 1);
				if (end == std::string::npos)
				{
					return false;
				}

				const std::string name = value.substr(i + 1, end - i - 1);
				if (!valid_placeholder_name(name))
				{
					return false;
				}

				++placeholders[name];
				i = end;
			}

			return true;
		}

		bool placeholders_match(const std::string& expected, const std::string& candidate)
		{
			std::unordered_map<std::string, int> expected_placeholders;
			std::unordered_map<std::string, int> candidate_placeholders;
			return collect_placeholders(expected, expected_placeholders) &&
				collect_placeholders(candidate, candidate_placeholders) &&
				expected_placeholders == candidate_placeholders;
		}

		bool parse_value(std::string raw, std::string& result)
		{
			raw = trim_copy(std::move(raw));
			if (raw.empty())
			{
				return false;
			}

			if (raw.front() != '"')
			{
				result = std::move(raw);
				return true;
			}

			if (raw.size() < 2 || raw.back() != '"')
			{
				return false;
			}

			result.clear();
			for (std::size_t i = 1; i + 1 < raw.size(); ++i)
			{
				if (raw[i] == '"')
				{
					return false;
				}
				if (raw[i] != '\\')
				{
					result.push_back(raw[i]);
					continue;
				}

				if (++i >= raw.size() - 1)
				{
					return false;
				}

				switch (raw[i])
				{
				case 'n': result.push_back('\n'); break;
				case 'r': result.push_back('\r'); break;
				case 't': result.push_back('\t'); break;
				case '\\': result.push_back('\\'); break;
				case '"': result.push_back('"'); break;
				default: return false;
				}
			}

			return !result.empty();
		}

		bundle build_compiled_bundle()
		{
			bundle result;
			for (const auto& entry : compiled_english)
			{
				result.emplace(entry.key, entry.value);
			}
			return result;
		}

		bundle load_external_bundle(const std::string& path, const bundle& fallback)
		{
			bundle result = fallback;
			const std::string contents = fs::read(path);
			if (contents.empty() || !valid_utf8(contents))
			{
				return result;
			}

			std::string text = contents;
			if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
				static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
			{
				text.erase(0, 3);
			}

			std::istringstream stream(text);
			std::string line;
			while (std::getline(stream, line))
			{
				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}

				const std::string trimmed = trim_copy(line);
				if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';')
				{
					continue;
				}

				const std::size_t equals = trimmed.find('=');
				if (equals == std::string::npos)
				{
					continue;
				}

				const std::string key = trim_copy(trimmed.substr(0, equals));
				const auto expected = fallback.find(key);
				if (key.empty() || expected == fallback.end())
				{
					continue;
				}

				std::string value;
				if (parse_value(trimmed.substr(equals + 1), value) && valid_utf8(value) &&
					placeholders_match(expected->second, value))
				{
					result[key] = std::move(value);
				}
			}

			return result;
		}

		const bundle& active_bundle()
		{
			return active_language == language::es ? cached_spanish : cached_english;
		}

		const char* compiled_text(const char* key)
		{
			for (const auto& entry : compiled_english)
			{
				if (std::strcmp(entry.key, key) == 0)
				{
					return entry.value;
				}
			}

			return key;
		}
	}

	void init(const language configured_language)
	{
		if (!bundles_loaded)
		{
			cached_english = build_compiled_bundle();
			cached_english = load_external_bundle(fs::get_self_path() + "ecm-r\\translations\\en.ini", cached_english);
			cached_spanish = load_external_bundle(fs::get_self_path() + "ecm-r\\translations\\es.ini", cached_english);
			bundles_loaded = true;
		}

		active_language = configured_language;
	}

	bool parse_language(const char* raw_value, language& result)
	{
		if (!raw_value)
		{
			return false;
		}

		std::string value = trim_copy(raw_value);
		if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
		{
			value = trim_copy(value.substr(1, value.size() - 2));
		}

		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});

		if (value == "en")
		{
			result = language::en;
			return true;
		}
		if (value == "es")
		{
			result = language::es;
			return true;
		}

		return false;
	}

	const char* code(const language value)
	{
		return value == language::es ? "es" : "en";
	}

	language current()
	{
		return active_language;
	}

	void set_language(const language value)
	{
		active_language = value;
	}

	const char* text(const char* key)
	{
		if (!key)
		{
			return "";
		}

		const auto& bundle = active_bundle();
		const auto translated = bundle.find(key);
		return translated != bundle.end() ? translated->second.c_str() : compiled_text(key);
	}

	std::string format(const char* key, const std::initializer_list<std::pair<const char*, std::string>> values)
	{
		const std::string source = text(key);
		std::string result;
		for (std::size_t i = 0; i < source.size(); ++i)
		{
			if (source[i] != '{')
			{
				result.push_back(source[i]);
				continue;
			}

			const std::size_t end = source.find('}', i + 1);
			if (end == std::string::npos)
			{
				result.append(source, i, std::string::npos);
				break;
			}

			const std::string name = source.substr(i + 1, end - i - 1);
			const auto replacement = std::find_if(values.begin(), values.end(), [&name](const auto& item) {
				return item.first && name == item.first;
			});
			if (replacement != values.end())
			{
				result += replacement->second;
			}
			else
			{
				result.append(source, i, end - i + 1);
			}
			i = end;
		}

		return result;
	}
}
