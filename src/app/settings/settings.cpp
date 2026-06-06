
#include "logger/logger.hpp"
#include "fs/fs.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "settings.hpp"
#include "menus/menus.hpp"
#include "hook/hook.hpp"
#include "audio/audio.hpp"
#include "input/input.hpp"

namespace
{
	// Returns a copy of the input without leading or trailing whitespace.
	std::string trim_copy(std::string value)
	{
		auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
		auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();

		if (first >= last)
		{
			return {};
		}

		return std::string(first, last);
	}

	// Normalizes [trax] routing values to the supported ALL, FE, or IG tokens.
	std::string normalize_trax_value(const char* raw_value)
	{
		if (!raw_value)
		{
			return "ALL";
		}

		std::string value = trim_copy(raw_value);

		if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
		{
			value = value.substr(1, value.size() - 2);
			value = trim_copy(value);
		}

		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
			return static_cast<char>(std::toupper(ch));
		});

		if (value == "FE" || value == "IG" || value == "ALL")
		{
			return value;
		}

		return "ALL";
	}

	// Reads an INI value while providing a stable fallback when the key is missing.
	const char* safe_ini_get(ini_t* config, const char* section, const char* key, const char* fallback)
	{
		const char* value = ini_get(config, section, key);
		return value ? value : fallback;
	}

	// Builds the version line persisted inside the [core] section.
	std::string build_version_line()
	{
		return std::string("version = \"") + VERSION + "\"";
	}

	// Saves a single INI value by loading, mutating, and writing the config file.
	bool save_ini_value(const char* section, const char* key, const std::string& value)
	{
		if (settings::config_file.empty())
		{
			return false;
		}

		ini_t* config = ini_load(settings::config_file.c_str());
		if (!config)
		{
			return false;
		}

		ini_set(config, section, key, value.c_str());
		const bool saved = ini_save(config, settings::config_file.c_str()) != 0;
		ini_free(config);
		return saved;
	}

	// Formats one persisted volume line for the generated config text.
	std::string build_volume_line(const char* key, const int volume)
	{
		return std::string(key) + " = \"" + std::to_string(volume) + "\"";
	}

	// Formats the persisted shuffle toggle line.
    std::string build_shuffle_enabled_line(const bool shuffle_enabled)
	{
		return std::string("shuffle_enabled = ") + (shuffle_enabled ? "true" : "false");
	}

	// Formats the persisted repeat toggle line.
	std::string build_repeat_enabled_line(const bool repeat_enabled)
	{
		return std::string("repeat_enabled = ") + (repeat_enabled ? "true" : "false");
	}

	// Formats the persisted loading-screen stop toggle line.
  std::string build_stop_music_on_loading_screens_line(const bool stop_music_on_loading_screens)
	{
		return std::string("stop_music_on_loading_screens = ") + (stop_music_on_loading_screens ? "true" : "false");
	}

	// Formats the persisted in-game movie muting toggle line.
	std::string build_ingame_movie_muting_line(const bool ingame_movie_muting)
	{
		return std::string("ingame_movie_muting = ") + (ingame_movie_muting ? "true" : "false");
	}

	// Updates only the stored config version while preserving the rest of the file layout.
	void update_config_version_only(const std::string& path)
	{
		const std::string contents = fs::read(path);
		if (contents.empty())
		{
			return;
		}

		const std::string newline = contents.find("\r\n") != std::string::npos ? "\r\n" : "\n";
		const bool has_trailing_newline = !contents.empty() && (contents.back() == '\n' || contents.back() == '\r');

		std::vector<std::string> lines;
		for (std::size_t start = 0; start < contents.size();)
		{
			const auto end = contents.find_first_of("\r\n", start);
			if (end == std::string::npos)
			{
				lines.emplace_back(contents.substr(start));
				break;
			}

			lines.emplace_back(contents.substr(start, end - start));
			start = end + 1;
			if (contents[end] == '\r' && start < contents.size() && contents[start] == '\n')
			{
				++start;
			}
		}

		std::size_t insert_index = lines.size();
		std::size_t version_index = lines.size();
		bool in_core = false;
		bool found_core = false;

		for (std::size_t i = 0; i < lines.size(); ++i)
		{
			const std::string trimmed = trim_copy(lines[i]);
			const bool is_section = trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']';

			if (is_section)
			{
				if (in_core)
				{
					insert_index = i;
					break;
				}

				in_core = trimmed == "[core]";
				found_core = found_core || in_core;
				continue;
			}

			if (!in_core)
			{
				continue;
			}

			const auto equals = lines[i].find('=');
			if (equals == std::string::npos)
			{
				continue;
			}

			if (trim_copy(lines[i].substr(0, equals)) == "version")
			{
				version_index = i;
				break;
			}
		}

		if (!found_core)
		{
			return;
		}

		const std::string version_line = build_version_line();
		if (version_index < lines.size())
		{
			lines[version_index] = version_line;
		}
		else
		{
			lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insert_index), version_line);
		}

		std::ostringstream output;
		for (std::size_t i = 0; i < lines.size(); ++i)
		{
			if (i > 0)
			{
				output << newline;
			}
			output << lines[i];
		}

		if (has_trailing_newline)
		{
			output << newline;
		}

		if (output.str() != contents)
		{
			fs::write(path, output.str(), false);
		}
	}

	// Rebuilds the complete INI text from the current runtime state.
	std::string build_config_text(const std::string& playlist, const int volume, const int frontend_volume, const int ingame_volume, const bool stop_music_on_loading_screens, const bool ingame_movie_muting, const bool shuffle_enabled, const bool repeat_enabled)
	{
		std::ostringstream output;
		output << "[core]\n";
		output << "playlist = \"" << playlist << "\"\n";
        output << build_volume_line("volume", volume) << "\n";
		output << build_volume_line("frontend_volume", frontend_volume) << "\n";
		output << build_volume_line("ingame_volume", ingame_volume) << "\n";
		output << "version = \"" << VERSION << "\"\n\n";
      output << "[config]\n";
		output << build_shuffle_enabled_line(shuffle_enabled) << "\n";
		output << build_repeat_enabled_line(repeat_enabled) << "\n";
		output << build_stop_music_on_loading_screens_line(stop_music_on_loading_screens) << "\n\n";
		output << "[experimental]\n";
		output << build_ingame_movie_muting_line(ingame_movie_muting) << "\n\n";
		output << "[keys]\n";
		for (const auto& binding : input::hotkey_bindings())
		{
			output << binding.ini_key << " = " << input::key_to_string(*binding.runtime_key) << "\n";
		}
		output << "\n[trax]\n";

		for (const auto& song : audio::playlist_files)
		{
			std::string file = song.first;
			file.erase(0, audio::playlist_dir.size() + 1);
			output << file << " = " << normalize_trax_value(song.second.c_str()) << "\n";
		}

		return output.str();
	}
}

void settings::init()
{
	settings::config_file = fs::get_self_path() + settings::config_file;

	settings::update();
}

void settings::update()
{
	ini_t* config;

	if (fs::exists(settings::config_file))
	{
		config = ini_load(&settings::config_file[0]);

		if (!config)
		{
			fs::del(settings::config_file);
			settings::update();
			return;
		}

		const bool version_changed = std::strcmp(safe_ini_get(config, "core", "version", ""), VERSION) != 0;
		const std::string legacy_volume = safe_ini_get(config, "core", "volume", "100");
		const bool missing_stop_music_on_loading_screens = ini_get(config, "config", "stop_music_on_loading_screens") == nullptr;
		const char* ingame_movie_muting_value = ini_get(config, "experimental", "ingame_movie_muting");
		const char* legacy_ingame_movie_muting_value = ini_get(config, "config", "experimental_ingame_movie_muting");
		const bool missing_ingame_movie_muting = ingame_movie_muting_value == nullptr;
		const bool legacy_ingame_movie_muting_present = legacy_ingame_movie_muting_value != nullptr;
		const bool missing_shuffle_enabled = ini_get(config, "config", "shuffle_enabled") == nullptr;
		const bool missing_repeat_enabled = ini_get(config, "config", "repeat_enabled") == nullptr;
		const bool missing_frontend_volume = ini_get(config, "core", "frontend_volume") == nullptr;
		const bool missing_ingame_volume = ini_get(config, "core", "ingame_volume") == nullptr;
		bool missing_hotkey_entry = false;
		bool invalid_hotkey_entry = false;

		audio::volume = std::stoi(legacy_volume);
		audio::frontend_volume = std::stoi(safe_ini_get(config, "core", "frontend_volume", legacy_volume.c_str()));
		audio::ingame_volume = std::stoi(safe_ini_get(config, "core", "ingame_volume", legacy_volume.c_str()));
		audio::shuffle_enabled = settings::get_boolean(safe_ini_get(config, "config", "shuffle_enabled", "true"));
		audio::repeat_enabled = settings::get_boolean(safe_ini_get(config, "config", "repeat_enabled", "true"));
		audio::stop_music_on_loading_screens = settings::get_boolean(safe_ini_get(config, "config", "stop_music_on_loading_screens", "true"));
		audio::ingame_movie_muting = settings::get_boolean(ingame_movie_muting_value ? ingame_movie_muting_value : (legacy_ingame_movie_muting_value ? legacy_ingame_movie_muting_value : "false"));
		input::reset_all_hotkeys();
		for (const auto& binding : input::hotkey_bindings())
		{
			const char* raw_value = ini_get(config, "keys", binding.ini_key);
			const std::string default_key_text = input::key_to_string(binding.default_key);
			const std::uint32_t parsed_key = input::key_from_string(raw_value ? raw_value : default_key_text.c_str(), binding.default_key);
			std::string error_message;
			if (!input::assign_hotkey(binding.action, parsed_key, &error_message))
			{
				input::reset_hotkey(binding.action);
				invalid_hotkey_entry = true;
			}

			if (!raw_value)
			{
				missing_hotkey_entry = true;
			}
		}

		audio::playlist_name = safe_ini_get(config, "core", "playlist", "Music");
		audio::playlist_dir = audio::playlist_name;
		audio::playlist_dir = fs::get_self_path() + audio::playlist_dir;
		audio::enumerate_playlist();

		for (int i = 0; i < audio::playlist_files.size(); ++i)
		{
			std::string song = audio::playlist_files[i].first;
			song.erase(0, audio::playlist_dir.size() + 1);
          const char* res = ini_get(config, "trax", song.c_str());
			audio::playlist_files[i].second = normalize_trax_value(res);
		}

		settings::sync_trax_entries();

		if (version_changed || missing_stop_music_on_loading_screens || missing_ingame_movie_muting || legacy_ingame_movie_muting_present || missing_shuffle_enabled || missing_repeat_enabled || missing_frontend_volume || missing_ingame_volume || missing_hotkey_entry || invalid_hotkey_entry)
		{
			fs::write(settings::config_file, build_config_text(audio::playlist_name, audio::volume, audio::frontend_volume, audio::ingame_volume, audio::stop_music_on_loading_screens, audio::ingame_movie_muting, audio::shuffle_enabled, audio::repeat_enabled), false);
		}
	}
	else if (!fs::exists(settings::config_file))
	{
		input::reset_all_hotkeys();

		audio::playlist_name = "Music";
		audio::playlist_dir = audio::playlist_name;
		audio::playlist_dir = fs::get_self_path() + audio::playlist_dir;
		audio::enumerate_playlist();

		for (int i = 0; i < audio::playlist_files.size(); ++i)
		{
          audio::playlist_files[i].second = "ALL";
		}

		audio::volume = 100;
        audio::frontend_volume = 100;
		audio::ingame_volume = 100;
		audio::shuffle_enabled = true;
		audio::repeat_enabled = true;
		audio::stop_music_on_loading_screens = true;
		audio::ingame_movie_muting = false;
		fs::write(settings::config_file, build_config_text(audio::playlist_name, audio::volume, audio::frontend_volume, audio::ingame_volume, audio::stop_music_on_loading_screens, audio::ingame_movie_muting, audio::shuffle_enabled, audio::repeat_enabled), false);
		return;
	}

	ini_free(config);
}

bool settings::save_core_integer(const char* key, const int value)
{
	return save_ini_value("core", key, std::to_string(value));
}

bool settings::save_config_boolean(const char* key, const bool value)
{
	return save_ini_value("config", key, value ? "true" : "false");
}

bool settings::save_experimental_boolean(const char* key, const bool value)
{
	return save_ini_value("experimental", key, value ? "true" : "false");
}

bool settings::save_hotkey_binding(const char* key_name, const std::uint32_t key)
{
	return save_ini_value("keys", key_name, input::key_to_string(key));
}

bool settings::save_all_hotkey_bindings()
{
	if (settings::config_file.empty())
	{
		return false;
	}

	ini_t* config = ini_load(settings::config_file.c_str());
	if (!config)
	{
		return false;
	}

	for (const auto& binding : input::hotkey_bindings())
	{
		ini_set(config, "keys", binding.ini_key, input::key_to_string(*binding.runtime_key).c_str());
	}

	const bool saved = ini_save(config, settings::config_file.c_str()) != 0;
	ini_free(config);
	return saved;
}

bool settings::sync_trax_entries()
{
	if (settings::config_file.empty())
	{
		return false;
	}

	ini_t* config = ini_load(settings::config_file.c_str());
	if (!config)
	{
		return false;
	}

	for (const auto& song : audio::playlist_files)
	{
		std::string file = song.first;
		file.erase(0, audio::playlist_dir.size() + 1);
		ini_set(config, "trax", file.c_str(), song.second.c_str());
	}

	// Remove orphaned [trax] entries for files no longer on disk
	{
		const std::string raw = fs::read(settings::config_file);
		if (!raw.empty())
		{
			bool in_trax = false;
			std::string line;
			std::istringstream stream(raw);
			while (std::getline(stream, line))
			{
				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}

				if (!line.empty() && line.front() == '[')
				{
					in_trax = (line == "[trax]");
					continue;
				}

				if (!in_trax)
				{
					continue;
				}

				const std::size_t eq_pos = line.find('=');
				if (eq_pos == std::string::npos)
				{
					continue;
				}

				const std::string key = trim_copy(line.substr(0, eq_pos));
				if (key.empty())
				{
					continue;
				}

				const std::string full_path = audio::playlist_dir + "\\" + key;
				if (!fs::exists(full_path))
				{
					ini_set(config, "trax", key.c_str(), "");
				}
			}
		}
	}

	const bool saved = ini_save(config, settings::config_file.c_str()) != 0;
	ini_free(config);
	return saved;
}

bool settings::get_boolean(const char* bool_text)
{
	if (!bool_text)
	{
		return false;
	}

	std::string value = trim_copy(bool_text);
	if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
	{
		value = value.substr(1, value.size() - 2);
		value = trim_copy(value);
	}

	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});

	return value == "true" || value == "1" || value == "yes" || value == "on";
}

//Hardcoded until x64 becomes useable
std::string settings::config_file = "ecm-r.x86.ini";
