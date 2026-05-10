#pragma once

#include <cstdint>

#include <ini_rw.h>

class settings
{
public:
	static void init();
	static void update();
	static bool get_boolean(const char* bool_text);
	static bool save_core_integer(const char* key, int value);
	static bool save_config_boolean(const char* key, bool value);
	static bool save_hotkey_binding(const char* key_name, std::uint32_t key);
	static bool save_all_hotkey_bindings();
	static std::string config_file;
};
