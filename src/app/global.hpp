#pragma once

#include "defs.hpp"

struct vec2
{
	int x, y;
};

enum game_t : int
{
	UNIVERSAL = -1,
	NFSU,
	NFSU2,
	NFSC,
	NFSPS,
	NFSUC,
};

/// Stores cross-cutting runtime state shared by hooks, audio, and overlay code.
class global
{
public:
	static bool shutdown;

	/// Shows a message box attached to the current game window.
	static void msg_box(std::string title, std::string message)
	{
		auto to_wide = [](const std::string& value)
		{
			if (value.empty())
			{
				return std::wstring();
			}

			int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
			UINT code_page = CP_UTF8;
			if (length <= 0)
			{
				code_page = CP_ACP;
				length = MultiByteToWideChar(code_page, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
			}

			if (length <= 0)
			{
				return std::wstring();
			}

			std::wstring result(static_cast<std::size_t>(length), L'\0');
			MultiByteToWideChar(code_page, code_page == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, value.data(), static_cast<int>(value.size()), result.data(), length);
			return result;
		};

		const std::wstring wide_title = to_wide(title);
		const std::wstring wide_message = to_wide(message);
		MessageBoxW(global::hwnd, wide_message.c_str(), wide_title.c_str(), 0);
	}

	static std::vector<std::string> game_bins;

	static HMODULE self;
	static bool sys_init;
	static game_t game;
	static bool hide;
	static HWND hwnd;
	static kiero::RenderType::Enum renderer;
	static GameFlowState global::state;
};
