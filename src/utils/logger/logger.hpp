#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "../app/global.hpp"

class logger
{
public:
	enum class level : std::uint8_t
	{
		error,
		warning,
		info,
		debug,
	};

	inline static constexpr std::uint64_t max_file_size = 2ull * 1024ull * 1024ull;

	// Starts the bootstrap sink once. A failed file sink stays disabled for this session.
	static void init()
	{
		std::lock_guard<std::mutex> lock(state_mutex());
		if (initialized())
		{
			return;
		}

		initialized() = true;
		current_level() = level::debug;

		if (!get_log_paths(active_path(), backup_path()))
		{
			disable_file_sink("could not prepare the log directory", GetLastError());
			return;
		}

		std::uint64_t existing_size = 0;
		if (!get_file_size(active_path(), existing_size))
		{
			disable_file_sink("could not inspect the active log", GetLastError());
			return;
		}

		if (existing_size > max_file_size)
		{
			if (!rotate_file())
			{
				return;
			}
		}
		else
		{
			if (!open_active_file(existing_size))
			{
				return;
			}

			const std::string header = session_header();
			if (file_bytes() + header.size() > max_file_size)
			{
				if (!rotate_file())
				{
					return;
				}
			}
			else if (!write_file_data(header))
			{
				disable_file_sink("could not write the session header", 0);
			}
		}
	}

	static void set_level(const level value)
	{
		std::lock_guard<std::mutex> lock(state_mutex());
		current_level() = value;
	}

	static level current_level_value()
	{
		std::lock_guard<std::mutex> lock(state_mutex());
		return current_level();
	}

	static const char* level_name(const level value)
	{
		switch (value)
		{
		case level::error:
			return "error";
		case level::warning:
			return "warning";
		case level::info:
			return "info";
		case level::debug:
			return "debug";
		}

		return "info";
	}

	static std::string normalize_level_text(const char* raw_value)
	{
		if (!raw_value)
		{
			return {};
		}

		std::string value(raw_value);
		trim_text(value);
		if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
		{
			value = value.substr(1, value.size() - 2);
			trim_text(value);
		}

		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
			return static_cast<char>(std::tolower(ch));
		});
		return value;
	}

	static bool parse_level(const char* raw_value, level& result)
	{
		const std::string value = normalize_level_text(raw_value);
		if (value == "error")
		{
			result = level::error;
			return true;
		}
		if (value == "warning")
		{
			result = level::warning;
			return true;
		}
		if (value == "info")
		{
			result = level::info;
			return true;
		}
		if (value == "debug")
		{
			result = level::debug;
			return true;
		}

		result = level::info;
		return false;
	}

	static void log(const std::string& type, const std::string& text)
	{
		write(level_for_type(type), type, text);
	}

	static void log_info(const std::string& text)
	{
		write(level::info, "INFO", text);
	}

	static void log_error(const std::string& text)
	{
		write(level::error, "ERROR", text);
	}

	static void log_warning(const std::string& text)
	{
		write(level::warning, "WARNING", text);
	}

	static void log_debug(const std::string& text)
	{
		write(level::debug, "DEBUG", text);
	}

	static std::string va(const char* fmt, ...)
	{
		va_list arguments;
		va_start(arguments, fmt);
		char result[512]{};
		const int written = fmt != nullptr ? std::vsnprintf(result, sizeof(result), fmt, arguments) : -1;
		va_end(arguments);

		if (written < 0)
		{
			return "<log format error>";
		}
		if (static_cast<std::size_t>(written) >= sizeof(result))
		{
			result[sizeof(result) - 4] = '.';
			result[sizeof(result) - 3] = '.';
			result[sizeof(result) - 2] = '.';
			result[sizeof(result) - 1] = '\0';
		}
		return std::string(result);
	}

#ifndef HELPER
	static std::string get_toggle(bool input)
	{
		switch (input)
		{
		case true:
			return "On";
		case false:
			return "Off";
		default:
			return "???";
		}
	}

	static std::vector<std::string> split(const std::string& s, const std::string& seperator)
	{
		std::vector<std::string> output;

		std::string::size_type prev_pos = 0, pos = 0;

		while ((pos = s.find(seperator, pos)) != std::string::npos)
		{
			std::string substring(s.substr(prev_pos, pos - prev_pos));
			output.push_back(substring);
			prev_pos = pos += seperator.length();
		}

		output.push_back(s.substr(prev_pos, pos - prev_pos));
		return output;
	}

	static void to_lower(std::string& string)
	{
		std::for_each(string.begin(), string.end(), ([](char& c)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}));
	}

	static void to_upper(std::string& string)
	{
		std::for_each(string.begin(), string.end(), ([](char& c)
		{
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		}));
	}

	static void trim(std::string& s)
	{
		trim_text(s);
	}

	static void remove_non_ascii(std::string& s)
	{
		s.erase(std::remove_if(s.begin(), s.end(), [](char c) { return !(c >= 0 && c < 128); }), s.end());
	}

	static std::string convert_codepage(std::string& in)
	{
		std::vector<wchar_t> wide;
		std::vector<char> normal;
		const int max_size = static_cast<int>(in.size()) * 4 + 1;

		wide.resize(max_size);
		normal.resize(max_size);

		WideCharToMultiByte(
			CP_UTF8,
			0,
			&wide[0],
			MultiByteToWideChar(1252, 0, in.c_str(), static_cast<int>(in.size()), &wide[0], max_size),
			&normal[0],
			max_size,
			nullptr,
			nullptr
		);

		return &normal[0];
	}

	static void rem_path_info(std::string& str, const std::string& dir)
	{
		// Assuming ext is 4 for now.
		str.erase(0, dir.size() + 1);
		str.erase(str.size() - 4, 4);
	}
#endif

private:
	static std::mutex& state_mutex()
	{
		static std::mutex value;
		return value;
	}

	static bool& initialized()
	{
		static bool value = false;
		return value;
	}

	static level& current_level()
	{
		static level value = level::debug;
		return value;
	}

	static bool& file_sink_enabled()
	{
		static bool value = true;
		return value;
	}

	static std::ofstream& file_stream()
	{
		static std::ofstream value;
		return value;
	}

	static std::string& active_path()
	{
		static std::string value;
		return value;
	}

	static std::string& backup_path()
	{
		static std::string value;
		return value;
	}

	static std::uint64_t& file_bytes()
	{
		static std::uint64_t value = 0;
		return value;
	}

	static void trim_text(std::string& value)
	{
		auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
		auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
		if (first >= last)
		{
			value.clear();
		}
		else
		{
			value = std::string(first, last);
		}
	}

	static level level_for_type(const std::string& type)
	{
		const std::string value = normalize_level_text(type.c_str());
		if (value == "error")
		{
			return level::error;
		}
		if (value == "warning")
		{
			return level::warning;
		}
		if (value == "debug")
		{
			return level::debug;
		}

		return level::info;
	}

	static std::string timestamp()
	{
		const std::time_t now = std::time(nullptr);
		std::tm local{};
		localtime_s(&local, &now);
		char result[32]{};
		std::strftime(result, sizeof(result), "%Y-%m-%d %H:%M:%S", &local);
		return result;
	}

	static std::string session_header()
	{
		return "\n--- ECM-R log session " + timestamp() + " (bootstrap=debug) ---\n";
	}

	static bool get_log_paths(std::string& active, std::string& backup)
	{
		char module_path[MAX_PATH]{};
		const DWORD length = GetModuleFileNameA(global::self, module_path, sizeof(module_path));
		if (length == 0 || length >= sizeof(module_path))
		{
			return false;
		}

		std::string directory(module_path, length);
		const std::size_t separator = directory.find_last_of("\\/");
		if (separator == std::string::npos)
		{
			return false;
		}

		directory.erase(separator + 1);
		std::string log_directory = directory + "ecm-r";
		if (!CreateDirectoryA(log_directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
		{
			return false;
		}

		const DWORD attributes = GetFileAttributesA(log_directory.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			return false;
		}
		if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			SetLastError(ERROR_DIRECTORY);
			return false;
		}

		log_directory += "\\";
		active = log_directory + "ecm-r.x86.log";
		backup = active + ".1";
		return true;
	}

	static bool get_file_size(const std::string& path, std::uint64_t& size)
	{
		WIN32_FILE_ATTRIBUTE_DATA attributes{};
		if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attributes))
		{
			if (GetLastError() == ERROR_FILE_NOT_FOUND)
			{
				size = 0;
				return true;
			}
			return false;
		}

		if ((attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			SetLastError(ERROR_ACCESS_DENIED);
			return false;
		}

		size = (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
		return true;
	}

	static void disable_file_sink(const char* reason, const DWORD error)
	{
		if (file_stream().is_open())
		{
			file_stream().close();
		}
		file_sink_enabled() = false;
		std::cout << "[ ERROR ] Persistent log disabled: " << reason;
		if (error != 0)
		{
			std::cout << " (Windows error " << error << ')';
		}
		std::cout << '\n';
	}

	static bool open_active_file(const std::uint64_t existing_size)
	{
		file_stream().clear();
		file_stream().open(active_path().c_str(), std::ios::binary | std::ios::out | std::ios::app);
		if (!file_stream().is_open())
		{
			disable_file_sink("could not open the active log", GetLastError());
			return false;
		}

		file_bytes() = existing_size;
		return true;
	}

	static bool write_file_data(const std::string& data)
	{
		if (!file_stream().is_open())
		{
			return false;
		}

		file_stream().write(data.data(), static_cast<std::streamsize>(data.size()));
		file_stream().flush();
		if (!file_stream())
		{
			return false;
		}

		file_bytes() += data.size();
		return true;
	}

	static bool restore_active_file()
	{
		DeleteFileA(active_path().c_str());
		return MoveFileA(backup_path().c_str(), active_path().c_str()) != FALSE;
	}

	static bool rotate_file()
	{
		if (file_stream().is_open())
		{
			file_stream().flush();
			if (!file_stream())
			{
				file_stream().close();
				disable_file_sink("could not flush the active log before rotation", 0);
				return false;
			}
			file_stream().close();
		}

		if (!DeleteFileA(backup_path().c_str()))
		{
			const DWORD error = GetLastError();
			if (error != ERROR_FILE_NOT_FOUND)
			{
				disable_file_sink("could not remove the previous log backup", error);
				return false;
			}
		}

		if (!MoveFileA(active_path().c_str(), backup_path().c_str()))
		{
			disable_file_sink("could not move the active log to its backup", GetLastError());
			return false;
		}

		file_stream().clear();
		file_stream().open(active_path().c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
		if (!file_stream().is_open())
		{
			const DWORD error = GetLastError();
			const bool restored = restore_active_file();
			disable_file_sink(restored ? "could not open the rotated active log" : "could not open or restore the rotated active log", error);
			return false;
		}

		file_bytes() = 0;
		const std::string header = session_header();
		if (header.size() > max_file_size || !write_file_data(header))
		{
			file_stream().close();
			DeleteFileA(active_path().c_str());
			const bool restored = restore_active_file();
			disable_file_sink(restored ? "could not write the rotated session header" : "could not write or restore the rotated session", 0);
			return false;
		}

		return true;
	}

	static std::string truncate_record(const std::string& record, const std::uint64_t available)
	{
		if (record.size() <= available)
		{
			return record;
		}

		static constexpr char marker[] = "... [truncated]\n";
		if (available <= sizeof(marker) - 1)
		{
			return record.substr(0, static_cast<std::size_t>(available));
		}

		const std::size_t prefix_size = static_cast<std::size_t>(available) - (sizeof(marker) - 1);
		return record.substr(0, prefix_size) + marker;
	}

	static void write(const level entry_level, const std::string& type, const std::string& text)
	{
		std::lock_guard<std::mutex> lock(state_mutex());
		if (entry_level > current_level())
		{
			return;
		}

		std::cout << "[ " << type << " ] " << text << '\n';

		if (!initialized() || !file_sink_enabled())
		{
			return;
		}

		const std::string record = "[ " + timestamp() + " ] [ " + type + " ] " + text + "\n";
		if (file_bytes() + record.size() > max_file_size)
		{
			if (!rotate_file())
			{
				return;
			}
		}

		const std::uint64_t available = max_file_size - file_bytes();
		const std::string safe_record = truncate_record(record, available);
		if (safe_record.empty() || !write_file_data(safe_record))
		{
			disable_file_sink("could not write a log record", 0);
		}
	}
};
