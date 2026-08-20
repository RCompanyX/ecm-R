#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

namespace localization
{
	enum class language : std::uint8_t
	{
		en,
		es,
	};

	/// Loads and validates the cached external bundles once per plugin startup.
	void init(language configured_language);
	/// Parses the persisted language code without changing the active locale.
	bool parse_language(const char* raw_value, language& result);
	/// Returns the canonical persisted language code.
	const char* code(language value);
	/// Returns the locale currently used by the overlay and startup dialogs.
	language current();
	/// Swaps the active cached bundle. External files are never read here.
	void set_language(language value);
	/// Returns a translated UTF-8 string, falling back to compiled English text.
	const char* text(const char* key);
	/// Replaces named placeholders in a translated UTF-8 string.
	std::string format(const char* key, std::initializer_list<std::pair<const char*, std::string>> values);
}
