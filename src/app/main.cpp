#include "global.hpp"
#include "logger/logger.hpp"
#include "input/input.hpp"
#include "menus/menus.hpp"
#include "audio/audio.hpp"
#include "settings/settings.hpp"
#include "fs/fs.hpp"
#include "hook/hook.hpp"

#include "hook/impl/d3d9_impl.h"
#include "hook/impl/d3d10_impl.h"
#include "hook/impl/d3d11_impl.h"
#include "hook/impl/opengl3_impl.h"

#include <cstring>

game_t game = game_t::NFSU2;

namespace
{
	const char* renderer_name(const kiero::RenderType::Enum renderer)
	{
		switch (renderer)
		{
		case kiero::RenderType::D3D9:
			return "D3D9";
		case kiero::RenderType::D3D10:
			return "D3D10";
		case kiero::RenderType::D3D11:
			return "D3D11";
		case kiero::RenderType::OpenGL:
			return "OpenGL";
		case kiero::RenderType::None:
			return "None";
		}

		return "Unknown";
	}
}

// Applies the earliest game-side mute patch once the engine has finished its system init.
void sys_init_()
{
	if (!global::sys_init)
	{
		logger::log_debug("NFSU2 system-init hook reached; muting game music paths");
	}
	global::sys_init = true;

	//Mute the in-game music
	switch (global::game)
	{
	case game_t::NFSU2:
		*(float*)(0x0083AA30) = 0.0f; //FE
		*(float*)(0x0083AA34) = 0.0f; //IG
		break;
	}
}

// Naked trampoline that injects ECM-R's system-init patch and returns to the game.
__declspec(naked) void sys_init()
{
	__asm
	{
		call sys_init_;
		push 0x0057EDA8;
		retn;
	}
}

// Main-loop hook that lets ECM-R update audio state once per game tick.
void __declspec(naked) NFSU2_MainLoop()
{
	static constexpr auto bGetTicker__Fv = 0x0043BDF0;

	__asm
	{
		call bGetTicker__Fv

		pushad;
		call audio::update;
		popad;

		push 0x005811E9;
		retn;
	}
}

static void(* sub_00537980_)(int a2, char* a3, int a4);

// Pauses or resumes custom music when specific UI packages trigger ECM-R's mute rules.
void sub_00537980(int a2, char* a3, int a4)
{
	if (audio::ingame_movie_muting)
	{
		sub_00537980_(a2, a3, a4);
		audio::sync_game_pause_from_mute_packages();
		return;
	}

	bool found = false;
	for (const char* package : audio::mute_detection)
	{
		if (!strcmp(package, a3))
		{
			found = true;
			break;
		}
	}

	if (found)
	{
		audio::pause();
	}
	else if (audio::game_paused)
	{
		audio::play();
	}

	return sub_00537980_(a2, a3, a4);
}

// Performs low-level runtime setup: patches, settings load, renderer hook selection, and MinHook enable.
void init()
{
	logger::log_debug("Bootstrap stage: initializing MinHook");
	const MH_STATUS minhook_status = MH_Initialize();
	if (minhook_status != MH_OK)
	{
		logger::log_error(logger::va("MinHook initialization failed (status %d)", static_cast<int>(minhook_status)));
	}
	else
	{
		logger::log_debug("MinHook initialized");
	}

	switch (game)
	{
	case game_t::NFSU2:
	{
		logger::log_debug("Bootstrap stage: applying NFSU2 patches and hooks");
		*(std::uint8_t*)(0x00534535) = 0xEB; //Prevent save from loading audio values

		//Disable sliders in menu
		hook::jump(0x004B6EDA, 0x004B6F92);

		//Disable sliders in-game menu
		hook::jump(0x004C347B, 0x004C3533);

		//Wait for sys init stub
		hook::jump(0x0057EDA3, sys_init);

		hook::jump(0x005811E4, NFSU2_MainLoop);

		const MH_STATUS package_hook_status = MH_CreateHook((void*)0x00537980, sub_00537980, (void**)&sub_00537980_);
		if (package_hook_status != MH_OK)
		{
			logger::log_error(logger::va("Package-load hook creation failed (status %d)", static_cast<int>(package_hook_status)));
		}
		else
		{
			logger::log_debug("NFSU2 package-load hook created");
		}
		logger::log_info("NFSU2 memory patches installed");
		break;
	}
	case game_t::UNIVERSAL:
		std::thread([] {
			while (!global::shutdown)
			{
				audio::update();
				std::this_thread::sleep_for(128ms);
			}
		});
		break;
	}

	logger::log_debug("Bootstrap stage: loading settings");
	settings::init();
	logger::log_debug("Settings initialization completed");

	logger::log_debug("Bootstrap stage: selecting renderer hooks");
	const auto kiero_status = kiero::init(kiero::RenderType::Auto);
	if (kiero_status == kiero::Status::Success)
	{
		const kiero::RenderType::Enum render_type = kiero::getRenderType();
		logger::log_info(logger::va("Renderer selected: %s", renderer_name(render_type)));
		switch (kiero::getRenderType())
		{
#if KIERO_INCLUDE_D3D9
		case kiero::RenderType::D3D9:
			impl::d3d9::init();
			break;
#endif

#if KIERO_INCLUDE_D3D10
		case kiero::RenderType::D3D10:
			impl::d3d10::init();
			break;
#endif

#if KIERO_INCLUDE_D3D11
		case kiero::RenderType::D3D11:
			impl::d3d11::init();
			break;
#endif

#if KIERO_INCLUDE_OPENGL
		case kiero::RenderType::OpenGL:
			impl::opengl3::init();
			break;
#endif

		case kiero::RenderType::None:
			logger::log_error("Renderer selection returned None; unloading ECM-R");
			FreeLibraryAndExitThread(global::self, 0);
			break;
		}
	}
	else
	{
		logger::log_error(logger::va("Renderer hook initialization failed (status %d)", static_cast<int>(kiero_status)));
	}

	const MH_STATUS enable_status = MH_EnableHook(MH_ALL_HOOKS);
	if (enable_status != MH_OK)
	{
		logger::log_error(logger::va("MinHook enable failed (status %d)", static_cast<int>(enable_status)));
	}
	else
	{
		logger::log_debug("MinHook hooks enabled");
	}
}

// Writes a crash dump for unexpected failures and reports the dump path to the user.
LONG WINAPI CustomUnhandledExceptionFilter(LPEXCEPTION_POINTERS ExceptionInfo)
{
	char module_path[MAX_PATH]{};
	const DWORD module_length = GetModuleFileNameA(global::self, module_path, sizeof(module_path));
	if (module_length == 0 || module_length >= sizeof(module_path))
	{
		char message[512]{};
		_snprintf_s(message, sizeof(message), _TRUNCATE, "Could not resolve the ECM-R module path for the crash dump (Windows error %lu).", GetLastError());
		MessageBoxA(nullptr, message, "ECM-R", MB_OK | MB_ICONERROR);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	__time64_t time_value{};
	tm local_time{};
	if (_time64(&time_value) == static_cast<__time64_t>(-1) || _localtime64_s(&local_time, &time_value) != 0)
	{
		MessageBoxA(nullptr, "Could not generate a crash dump filename.", "ECM-R", MB_OK | MB_ICONERROR);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	char timestamp[32]{};
	if (strftime(timestamp, std::size(timestamp), "%Y%m%d%H%M%S", &local_time) == 0)
	{
		MessageBoxA(nullptr, "Could not format the crash dump filename.", "ECM-R", MB_OK | MB_ICONERROR);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	const char* separator = strrchr(module_path, '\\');
	if (separator == nullptr)
	{
		separator = strrchr(module_path, '/');
	}
	if (separator == nullptr)
	{
		MessageBoxA(nullptr, "Could not resolve the crash dump directory.", "ECM-R", MB_OK | MB_ICONERROR);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	const std::size_t directory_length = static_cast<std::size_t>(separator - module_path) + 1;
	char dump_path[MAX_PATH]{};
	if (directory_length >= sizeof(dump_path))
	{
		MessageBoxA(nullptr, "The crash dump path is too long.", "ECM-R", MB_OK | MB_ICONERROR);
		return EXCEPTION_EXECUTE_HANDLER;
	}
	memcpy(dump_path, module_path, directory_length);

	HANDLE dump_file = INVALID_HANDLE_VALUE;
	DWORD open_error = ERROR_SUCCESS;
	for (unsigned int collision = 0; collision < 100; ++collision)
	{
		const int written = collision == 0
			? _snprintf_s(dump_path + directory_length, sizeof(dump_path) - directory_length, _TRUNCATE, "ecm-r-%s.dmp", timestamp)
			: _snprintf_s(dump_path + directory_length, sizeof(dump_path) - directory_length, _TRUNCATE, "ecm-r-%s-%u.dmp", timestamp, collision);
		if (written < 0)
		{
			open_error = ERROR_BUFFER_OVERFLOW;
			break;
		}

		dump_file = CreateFileA(dump_path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (dump_file != INVALID_HANDLE_VALUE)
		{
			break;
		}

		open_error = GetLastError();
		if (open_error != ERROR_FILE_EXISTS && open_error != ERROR_ALREADY_EXISTS)
		{
			break;
		}
	}

	bool dump_written = false;
	DWORD dump_error = open_error;
	if (dump_file != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION exception_data{};
		exception_data.ThreadId = GetCurrentThreadId();
		exception_data.ExceptionPointers = ExceptionInfo;
		exception_data.ClientPointers = FALSE;

#if defined(DEBUG)
		const MINIDUMP_TYPE dump_type = static_cast<MINIDUMP_TYPE>(MiniDumpWithProcessThreadData | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo | MiniDumpWithFullMemory);
#else
		const MINIDUMP_TYPE dump_type = MiniDumpNormal;
#endif

		dump_written = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump_file, dump_type, &exception_data, nullptr, nullptr) != FALSE;
		if (!dump_written)
		{
			dump_error = GetLastError();
		}
		CloseHandle(dump_file);
	}

	char message[1024]{};
	if (dump_written)
	{
		_snprintf_s(message, sizeof(message), _TRUNCATE, "A minidump has been written to %s.", dump_path);
	}
	else
	{
		_snprintf_s(message, sizeof(message), _TRUNCATE, "Could not write the minidump to %s (Windows error %lu).", dump_path, dump_error);
	}
	MessageBoxA(nullptr, message, "ECM-R", MB_OK | MB_ICONERROR);

	return EXCEPTION_EXECUTE_HANDLER;
}

// Worker-thread entry point that prepares the debug console and starts ECM-R.
DWORD WINAPI OnAttachImpl(LPVOID lpParameter)
{
	std::ios_base::sync_with_stdio(false);

	AllocConsole();
  SetConsoleTitleA("ECM-R Debug Console");


	std::freopen("CONOUT$", "w", stdout);
	std::freopen("CONIN$", "r", stdin);

	//For WHATEVER REASON, the mod fails to load properly without the console allocated
	//so we hide it here instead...
#ifdef NDEBUG
	ShowWindow(GetConsoleWindow(), 0);
#endif

	logger::init();
	logger::log_info("ECM-R bootstrap logger initialized");
	logger::log_debug("Bootstrap stage: console preparation completed");
	global::game = game_t::NFSU2;


	/*int found = -1;
	for (int i = 0; i < global::game_bins.size(); ++i)
	{
		if (fs::exists(global::game_bins[i]))
		{
			found = i;
			break;
		}
	}

	if (found == -1)
	{
		global::game = game_t::UNIVERSAL;
		logger::log_info("No game found! Switching to universal mode.");
	}
	else
	{
		global::game = (game_t)found;
		logger::log_info(logger::va("Game = %i", global::game));
	}*/

	init();
	return 0;
}

// Crash-guarded wrapper around the worker-thread bootstrap.
DWORD WINAPI OnAttach(LPVOID lpParameter)
{
	__try
	{
		return OnAttachImpl(lpParameter);
	}
	__except (CustomUnhandledExceptionFilter(GetExceptionInformation()))
	{
		FreeLibraryAndExitThread((HMODULE)lpParameter, 0xDECEA5ED);
	}

	return 0;
}

// DLL entry point that spawns the detached ECM-R startup thread on process attach.
BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		global::self = hModule;
		DisableThreadLibraryCalls(global::self);
		CreateThread(nullptr, 0, OnAttach, global::self, 0, nullptr);
		return true;
	}

	return false;
}
