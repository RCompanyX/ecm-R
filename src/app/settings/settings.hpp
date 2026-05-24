#pragma once

#include <cstdint>

#include <ini_rw.h>

/// Loads, migrates, and persists ECM-R configuration values.
class settings
{
public:
	/// Creates or migrates the INI file and loads runtime settings into the app subsystems.
	static void init();
	/// Persists the current runtime settings back to disk.
	static void update();
	/// Parses the repository's loose boolean text values used in the INI file.
	static bool get_boolean(const char* bool_text);
	/// Saves a numeric value inside the [core] section.
	static bool save_core_integer(const char* key, int value);
	/// Saves a boolean value inside the [config] section.
	static bool save_config_boolean(const char* key, bool value);
	/// Saves a boolean value inside the [experimental] section.
	static bool save_experimental_boolean(const char* key, bool value);
	/// Saves one hotkey binding inside the [keys] section.
	static bool save_hotkey_binding(const char* key_name, std::uint32_t key);
	/// Writes every current hotkey binding to the INI file.
	static bool save_all_hotkey_bindings();
	static std::string config_file;
};
