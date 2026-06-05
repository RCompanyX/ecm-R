#pragma once

#include <fstream>
#include <sstream>
#include <filesystem>
#include <Windows.h>

class fs
{
public:
	static std::string wstring_to_utf8(const std::wstring& wstr)
	{
		if (wstr.empty()) return {};
		const int len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
		if (len <= 0) return {};
		std::string result(len, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), len, nullptr, nullptr);
		return result;
	}

	static std::wstring utf8_to_wstring(const std::string& str)
	{
		if (str.empty()) return {};
		const int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
		if (len <= 0) return {};
		std::wstring result(len, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), len);
		return result;
	}

	static std::string utf8_to_ansi(const std::string& utf8_str)
	{
		if (utf8_str.empty()) return {};
		const int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), nullptr, 0);
		if (wide_len <= 0) return {};
		std::wstring wide(static_cast<size_t>(wide_len), 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8_str.data(), static_cast<int>(utf8_str.size()), wide.data(), wide_len);
		const int ansi_len = WideCharToMultiByte(CP_ACP, 0, wide.data(), wide_len, nullptr, 0, nullptr, nullptr);
		if (ansi_len <= 0) return {};
		std::string ansi(ansi_len, 0);
		WideCharToMultiByte(CP_ACP, 0, wide.data(), wide_len, ansi.data(), ansi_len, nullptr, nullptr);
		return ansi;
	}

	static bool exists(const std::string& path)
	{
		return std::filesystem::exists(utf8_to_wstring(path));
	}

	static std::string get_cur_dir()
	{
		return std::filesystem::current_path().string() + "\\";
	}

	static void write(const std::string& path, const std::string& contents, const bool append)
	{
		std::ofstream stream(path, std::ios::binary | std::ofstream::out | (append ? std::ofstream::app : 0));

		if (stream.is_open())
		{
			stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
			stream.close();
		}
	}

	static std::string read(const std::string& path)
	{
		std::ifstream in(path);
		std::ostringstream out;
		out << in.rdbuf();
		return out.str();
	}

	static void del(const std::string& path, bool folder = false)
	{
		if (!fs::exists(path)) return;

		switch (folder)
		{
		case false:
			std::filesystem::remove(path);
			break;
		case true:
			std::filesystem::remove_all(path);
			break;
		}
	}

	static void move(const std::string& path, const std::string& new_path, bool create_root = true)
	{
		if (create_root)
		{
			std::filesystem::create_directory(new_path);
		}

		for (const std::filesystem::path& p : std::filesystem::directory_iterator(path))
		{
			std::filesystem::path dest = new_path / p.filename();

			if (fs::exists(path))
			{
				if (std::filesystem::is_directory(p))
				{
					std::filesystem::create_directory(dest);
					fs::move(&p.string()[0], &dest.string()[0], false);
				}
				else
				{
					std::filesystem::rename(p, dest);
				}
			}
		}
	}

	static std::vector<std::string> get_all_files(const std::string& path, const std::initializer_list<std::string>& exts)
	{
		std::vector<std::string> retn;
		const std::wstring wpath = utf8_to_wstring(path);

		if (std::filesystem::exists(wpath) && std::filesystem::is_directory(wpath))
		{
			std::vector<std::wstring> wexts;
			for (const std::string& ex : exts)
				wexts.emplace_back(utf8_to_wstring("." + ex));

			for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(wpath))
			{
				if (std::filesystem::is_regular_file(entry))
				{
					const std::wstring wext = entry.path().extension().wstring();
					for (const std::wstring& wx : wexts)
					{
						if (lstrcmpiW(wext.c_str(), wx.c_str()) == 0)
						{
							retn.emplace_back(wstring_to_utf8(entry.path().wstring()));
							break;
						}
					}
				}
			}
		}

		return retn;
	}

	static std::string get_self_path()
	{
		static std::string mod_path;

		if (mod_path.empty())
		{
			char exe_name[512];
			GetModuleFileNameA(global::self, exe_name, sizeof(exe_name));

			char* exe_base_name = strrchr(exe_name, '\\');
			exe_base_name[0] = L'\0';

			mod_path = exe_name;
			mod_path += "\\";

			GetFullPathNameA(mod_path.c_str(), sizeof(exe_name), exe_name, nullptr);

			mod_path = exe_name;
			mod_path += "\\";
		}

		return mod_path;
	}
};
