#if KIERO_INCLUDE_D3D9

#include "d3d9_impl.h"
#include <d3d9.h>
#include "logger/logger.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace
{
	using direct3d_create9 = IDirect3D9*(WINAPI*)(UINT);
	using get_proc_address = FARPROC(WINAPI*)(HMODULE, LPCSTR);
	using create_device = HRESULT(WINAPI*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
	using reset = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
	using present = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
	using end_scene = HRESULT(WINAPI*)(IDirect3DDevice9*);

	constexpr std::uint16_t create_device_index = 16;
	constexpr std::uint16_t reset_index = 16;
	constexpr std::uint16_t present_index = 17;
	constexpr std::uint16_t end_scene_index = 42;

	std::atomic<direct3d_create9> direct3d_create9_provider{ nullptr };
	get_proc_address oGetProcAddress = nullptr;
	create_device oCreateDevice = nullptr;
	reset oReset = nullptr;
	present oPresent = nullptr;
	end_scene oEndScene = nullptr;

	void** direct3d_create9_import_slot = nullptr;
	void** get_proc_address_import_slot = nullptr;
	void* create_device_target = nullptr;
	void* reset_target = nullptr;
	void* present_target = nullptr;
	void* end_scene_target = nullptr;

	std::mutex hook_mutex;
	std::mutex runtime_mutex;
	bool create_device_hook_attempted = false;
	bool device_hook_attempted = false;
	std::atomic_bool device_hooks_bound{ false };
	std::atomic_bool imgui_initialized{ false };
	std::atomic_bool direct3d_create_call_site_seen{ false };
	std::atomic_bool factory_callback_seen{ false };
	std::atomic_bool create_device_callback_seen{ false };
	std::atomic_bool frame_callback_logged{ false };

	enum class frame_callback : std::uint8_t
	{
		none,
		end_scene,
		present,
	};

	std::atomic<frame_callback> selected_frame_callback{ frame_callback::none };

	IDirect3D9* WINAPI hkDirect3DCreate9CallSite(UINT sdk_version);
	FARPROC WINAPI hkGetProcAddress(HMODULE module, LPCSTR name);
	HRESULT WINAPI hkCreateDevice(IDirect3D9* direct3d9, UINT adapter, D3DDEVTYPE device_type, HWND focus_window,
		DWORD behavior_flags, D3DPRESENT_PARAMETERS* presentation_parameters, IDirect3DDevice9** returned_device);
	HRESULT WINAPI hkReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentation_parameters);
	HRESULT WINAPI hkPresent(IDirect3DDevice9* device, const RECT* source_rect, const RECT* destination_rect,
		HWND destination_window, const RGNDATA* dirty_region);
	HRESULT WINAPI hkEndScene(IDirect3DDevice9* device);

	void fail_renderer(const char* message)
	{
		if (!global::shutdown.exchange(true, std::memory_order_acq_rel))
		{
			logger::log_error(message);
		}
	}

	bool enable_hook(void* target)
	{
		const MH_STATUS status = MH_EnableHook(target);
		return status == MH_OK || status == MH_ERROR_ENABLED;
	}

	void remove_hook(void*& target)
	{
		if (target == nullptr)
		{
			return;
		}

		MH_DisableHook(target);
		MH_RemoveHook(target);
		target = nullptr;
	}

	void** find_import_slot(HMODULE module, const char* imported_module_name, const char* imported_function_name)
	{
		if (module == nullptr || imported_function_name == nullptr)
		{
			return nullptr;
		}

		auto* base = reinterpret_cast<std::uint8_t*>(module);
		auto* dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		if (dos_header->e_magic != IMAGE_DOS_SIGNATURE || dos_header->e_lfanew <= 0)
		{
			return nullptr;
		}

		auto* nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos_header->e_lfanew);
		if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
		{
			return nullptr;
		}
		if (nt_headers->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT)
		{
			return nullptr;
		}

		const IMAGE_DATA_DIRECTORY& import_directory = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (import_directory.VirtualAddress == 0)
		{
			return nullptr;
		}

		auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + import_directory.VirtualAddress);
		for (; descriptor->Name != 0; ++descriptor)
		{
			const char* current_module_name = reinterpret_cast<const char*>(base + descriptor->Name);
			if (imported_module_name != nullptr && _stricmp(current_module_name, imported_module_name) != 0)
			{
				continue;
			}

			if (descriptor->FirstThunk == 0)
			{
				continue;
			}

			auto* address_thunks = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
			if (descriptor->OriginalFirstThunk == 0)
			{
				HMODULE imported_module = GetModuleHandleA(current_module_name);
				const FARPROC imported_function = imported_module == nullptr
					? nullptr
					: ::GetProcAddress(imported_module, imported_function_name);
				if (imported_function == nullptr)
				{
					continue;
				}

				for (std::size_t index = 0; address_thunks[index].u1.Function != 0; ++index)
				{
					if (reinterpret_cast<FARPROC>(address_thunks[index].u1.Function) == imported_function)
					{
						return reinterpret_cast<void**>(&address_thunks[index].u1.Function);
					}
				}

				continue;
			}

			auto* name_thunks = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->OriginalFirstThunk);
			for (std::size_t index = 0; address_thunks[index].u1.Function != 0; ++index)
			{
				if ((name_thunks[index].u1.Ordinal & IMAGE_ORDINAL_FLAG) != 0)
				{
					continue;
				}

				const auto* import = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + name_thunks[index].u1.AddressOfData);
				if (_stricmp(reinterpret_cast<const char*>(import->Name), imported_function_name) == 0)
				{
					return reinterpret_cast<void**>(&address_thunks[index].u1.Function);
				}
			}
		}

		return nullptr;
	}

	bool patch_import_slot(void** slot, void* replacement, void** original)
	{
		if (slot == nullptr || replacement == nullptr || original == nullptr || *slot == nullptr || *slot == replacement)
		{
			return false;
		}

		DWORD old_protection = 0;
		if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protection))
		{
			logger::log_error(logger::va("D3D9 import call-site protection change failed (Windows error %lu)", GetLastError()));
			return false;
		}

		*original = InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(slot), replacement);
		DWORD restored_protection = 0;
		if (!VirtualProtect(slot, sizeof(void*), old_protection, &restored_protection))
		{
			logger::log_warning(logger::va("D3D9 import call-site protection restore failed (Windows error %lu)", GetLastError()));
		}
		FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
		return *original != nullptr;
	}

	void restore_import_slot(void** slot, void* replacement, void* original)
	{
		if (slot == nullptr || replacement == nullptr || original == nullptr)
		{
			return;
		}

		DWORD old_protection = 0;
		if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protection))
		{
			return;
		}

		if (*slot == replacement)
		{
			InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(slot), original);
		}

		DWORD restored_protection = 0;
		VirtualProtect(slot, sizeof(void*), old_protection, &restored_protection);
		FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));
	}

	bool is_d3d9_module(HMODULE module)
	{
		return module != nullptr && module == GetModuleHandleA("d3d9.dll");
	}

	bool is_named_proc(LPCSTR name, const char* expected)
	{
		return name != nullptr && !IS_INTRESOURCE(name) && _stricmp(name, expected) == 0;
	}

	bool read_vtable_entry(void* object, const std::uint16_t index, void** target)
	{
		if (object == nullptr || target == nullptr)
		{
			return false;
		}

		__try
		{
			void** vtable = *reinterpret_cast<void***>(object);
			*target = vtable == nullptr ? nullptr : vtable[index];
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			*target = nullptr;
		}

		return *target != nullptr;
	}

	bool bind_live_device(IDirect3DDevice9* device)
	{
		if (device == nullptr || global::shutdown.load(std::memory_order_acquire))
		{
			return false;
		}

		if (device_hooks_bound.load(std::memory_order_acquire))
		{
			return true;
		}

		std::lock_guard<std::mutex> lock(hook_mutex);
		if (device_hooks_bound.load(std::memory_order_acquire))
		{
			return true;
		}
		if (device_hook_attempted)
		{
			return false;
		}
		device_hook_attempted = true;

		void* reset_entry = nullptr;
		void* present_entry = nullptr;
		void* end_scene_entry = nullptr;
		if (!read_vtable_entry(device, reset_index, &reset_entry) ||
			!read_vtable_entry(device, present_index, &present_entry) ||
			!read_vtable_entry(device, end_scene_index, &end_scene_entry))
		{
			logger::log_error("D3D9 live device vtable entries are unavailable; renderer disabled");
			return false;
		}

		const MH_STATUS reset_status = MH_CreateHook(reset_entry, reinterpret_cast<LPVOID>(&hkReset), reinterpret_cast<LPVOID*>(&oReset));
		if (reset_status != MH_OK)
		{
			logger::log_error(logger::va("D3D9 live Reset hook failed (status %d)", static_cast<int>(reset_status)));
			return false;
		}
		reset_target = reset_entry;

		const MH_STATUS present_status = MH_CreateHook(present_entry, reinterpret_cast<LPVOID>(&hkPresent), reinterpret_cast<LPVOID*>(&oPresent));
		if (present_status != MH_OK)
		{
			remove_hook(reset_target);
			oReset = nullptr;
			logger::log_error(logger::va("D3D9 live Present hook failed (status %d)", static_cast<int>(present_status)));
			return false;
		}
		present_target = present_entry;

		const MH_STATUS end_scene_status = MH_CreateHook(end_scene_entry, reinterpret_cast<LPVOID>(&hkEndScene), reinterpret_cast<LPVOID*>(&oEndScene));
		if (end_scene_status != MH_OK)
		{
			remove_hook(reset_target);
			remove_hook(present_target);
			oReset = nullptr;
			oPresent = nullptr;
			logger::log_error(logger::va("D3D9 live EndScene hook failed (status %d)", static_cast<int>(end_scene_status)));
			return false;
		}
		end_scene_target = end_scene_entry;

		if (!enable_hook(reset_target) || !enable_hook(present_target) || !enable_hook(end_scene_target))
		{
			remove_hook(reset_target);
			remove_hook(present_target);
			remove_hook(end_scene_target);
			oReset = nullptr;
			oPresent = nullptr;
			oEndScene = nullptr;
			logger::log_error("D3D9 live device hooks could not be enabled; renderer disabled");
			return false;
		}

		device_hooks_bound.store(true, std::memory_order_release);
		logger::log_debug("D3D9 live device hooks bound (Reset/Present/EndScene)");
		return true;
	}

	bool bind_create_device(IDirect3D9* direct3d9)
	{
		if (direct3d9 == nullptr || global::shutdown.load(std::memory_order_acquire))
		{
			return false;
		}

		std::lock_guard<std::mutex> lock(hook_mutex);
		if (create_device_hook_attempted)
		{
			return oCreateDevice != nullptr;
		}
		create_device_hook_attempted = true;

		void* target = nullptr;
		if (!read_vtable_entry(direct3d9, create_device_index, &target))
		{
			logger::log_error("D3D9 live CreateDevice vtable entry is unavailable; renderer disabled");
			return false;
		}
		const MH_STATUS status = MH_CreateHook(target, reinterpret_cast<LPVOID>(&hkCreateDevice), reinterpret_cast<LPVOID*>(&oCreateDevice));
		if (status != MH_OK)
		{
			logger::log_error(logger::va("D3D9 live CreateDevice hook failed (status %d)", static_cast<int>(status)));
			oCreateDevice = nullptr;
			return false;
		}
		create_device_target = target;
		if (!enable_hook(target))
		{
			remove_hook(create_device_target);
			logger::log_error("D3D9 live CreateDevice hook could not be enabled");
			oCreateDevice = nullptr;
			return false;
		}

		logger::log_debug("D3D9 live CreateDevice hook bound; waiting for the game device");
		return true;
	}

	HRESULT call_create_device_safely(IDirect3D9* direct3d9, UINT adapter, D3DDEVTYPE device_type, HWND focus_window,
		DWORD behavior_flags, D3DPRESENT_PARAMETERS* presentation_parameters, IDirect3DDevice9** returned_device,
		bool* faulted)
	{
		HRESULT result = D3DERR_INVALIDCALL;
		*faulted = false;
		__try
		{
			result = oCreateDevice(direct3d9, adapter, device_type, focus_window, behavior_flags,
				presentation_parameters, returned_device);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			*faulted = true;
		}
		return result;
	}

	void capture_factory(IDirect3D9* direct3d9)
	{
		if (direct3d9 == nullptr)
		{
			return;
		}

		if (!factory_callback_seen.exchange(true, std::memory_order_acq_rel))
		{
			logger::log_info("D3D9 factory callback received");
		}

		if (!global::shutdown.load(std::memory_order_acquire) && !bind_create_device(direct3d9))
		{
			fail_renderer("D3D9 live factory CreateDevice capture failed; renderer disabled");
		}
	}

	IDirect3D9* WINAPI hkDirect3DCreate9CallSite(UINT sdk_version)
	{
		if (!direct3d_create_call_site_seen.exchange(true, std::memory_order_acq_rel))
		{
			logger::log_info("D3D9 Direct3DCreate9 call-site callback received");
		}

		const direct3d_create9 provider = direct3d_create9_provider.load(std::memory_order_acquire);
		if (provider == nullptr)
		{
			fail_renderer("D3D9 Direct3DCreate9 call-site has no provider function; renderer disabled");
			return nullptr;
		}

		// The provider call and its return value remain untouched; capture happens after it returns.
		IDirect3D9* direct3d9 = provider(sdk_version);
		capture_factory(direct3d9);
		return direct3d9;
	}

	FARPROC WINAPI hkGetProcAddress(HMODULE module, LPCSTR name)
	{
		if (oGetProcAddress == nullptr)
		{
			return nullptr;
		}

		const FARPROC result = oGetProcAddress(module, name);
		if (result != nullptr && is_d3d9_module(module) && is_named_proc(name, "Direct3DCreate9"))
		{
			direct3d_create9_provider.store(reinterpret_cast<direct3d_create9>(result), std::memory_order_release);
			return reinterpret_cast<FARPROC>(&hkDirect3DCreate9CallSite);
		}

		return result;
	}

	HRESULT WINAPI hkCreateDevice(IDirect3D9* direct3d9, UINT adapter, D3DDEVTYPE device_type, HWND focus_window,
		DWORD behavior_flags, D3DPRESENT_PARAMETERS* presentation_parameters, IDirect3DDevice9** returned_device)
	{
		if (!create_device_callback_seen.exchange(true, std::memory_order_acq_rel))
		{
			logger::log_info("D3D9 live CreateDevice callback received");
		}

		if (oCreateDevice == nullptr)
		{
			return D3DERR_INVALIDCALL;
		}

		bool faulted = false;
		const HRESULT result = call_create_device_safely(direct3d9, adapter, device_type, focus_window,
			behavior_flags, presentation_parameters, returned_device, &faulted);
		if (faulted)
		{
			fail_renderer("D3D9 live CreateDevice callback faulted; renderer disabled");
			return D3DERR_INVALIDCALL;
		}

		if (SUCCEEDED(result))
		{
			if (returned_device == nullptr || *returned_device == nullptr)
			{
				fail_renderer("D3D9 CreateDevice returned no COM device; renderer disabled");
			}
			else if (!global::shutdown.load(std::memory_order_acquire) && !bind_live_device(*returned_device))
			{
				fail_renderer("D3D9 live device capture failed; renderer disabled");
			}
		}

		return result;
	}

	HWND get_device_window(IDirect3DDevice9* device)
	{
		D3DDEVICE_CREATION_PARAMETERS parameters{};
		const HRESULT parameters_result = device->GetCreationParameters(&parameters);
		if (SUCCEEDED(parameters_result) && IsWindow(parameters.hFocusWindow))
		{
			return parameters.hFocusWindow;
		}
		if (FAILED(parameters_result))
		{
			logger::log_warning(logger::va("D3D9 GetCreationParameters failed (HRESULT 0x%08lX); trying the swap chain window",
				static_cast<unsigned long>(parameters_result)));
		}

		IDirect3DSwapChain9* swap_chain = nullptr;
		if (SUCCEEDED(device->GetSwapChain(0, &swap_chain)) && swap_chain != nullptr)
		{
			D3DPRESENT_PARAMETERS presentation_parameters{};
			if (SUCCEEDED(swap_chain->GetPresentParameters(&presentation_parameters)) && IsWindow(presentation_parameters.hDeviceWindow))
			{
				const HWND window = presentation_parameters.hDeviceWindow;
				swap_chain->Release();
				return window;
			}
			swap_chain->Release();
		}

		return nullptr;
	}

	void initialize_runtime(IDirect3DDevice9* device)
	{
		std::lock_guard<std::mutex> lock(runtime_mutex);
		if (imgui_initialized.load(std::memory_order_acquire) || global::shutdown.load(std::memory_order_acquire))
		{
			return;
		}

		if (device == nullptr)
		{
			fail_renderer("D3D9 live callback supplied a null device");
			return;
		}

		const HWND window = get_device_window(device);
		if (!IsWindow(window))
		{
			fail_renderer("D3D9 live device returned no valid game window");
			return;
		}

		global::hwnd = window;
		global::renderer = kiero::RenderType::Enum::D3D9;
		logger::log_info("D3D9 first-frame runtime initialization");
		audio::init();
		if (!audio::is_ready())
		{
			return;
		}

		input::init_overlay();
		menus::init();

		const bool win32_initialized = ImGui_ImplWin32_Init(window);
		const bool dx9_initialized = win32_initialized && ImGui_ImplDX9_Init(device);
		if (!win32_initialized || !dx9_initialized)
		{
			if (win32_initialized)
			{
				ImGui_ImplWin32_Shutdown();
			}
			fail_renderer("D3D9 ImGui backend initialization failed; audio disabled");
			return;
		}

		imgui_initialized.store(true, std::memory_order_release);
	}

	bool select_frame_callback(const frame_callback callback)
	{
		frame_callback expected = frame_callback::none;
		if (selected_frame_callback.compare_exchange_strong(expected, callback,
			std::memory_order_acq_rel, std::memory_order_acquire))
		{
			return true;
		}

		return expected == callback;
	}

	void render_frame(IDirect3DDevice9* device, const frame_callback callback)
	{
		if (!select_frame_callback(callback))
		{
			return;
		}
		global::renderer_callback_seen.store(true, std::memory_order_release);

		if (!frame_callback_logged.exchange(true, std::memory_order_acq_rel))
		{
			logger::log_info(callback == frame_callback::present
				? "D3D9 live Present callback received"
				: "D3D9 live EndScene callback received");
		}

		initialize_runtime(device);
		if (imgui_initialized.load(std::memory_order_acquire) && !global::shutdown.load(std::memory_order_acquire))
		{
			input::update();
			menus::prepare();
			menus::update();
			menus::present();
		}
	}

HRESULT WINAPI hkReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* presentation_parameters)
{
	if (oReset == nullptr || device == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	const bool can_use_imgui = imgui_initialized.load(std::memory_order_acquire) &&
		!global::shutdown.load(std::memory_order_acquire);
	if (can_use_imgui)
	{
		ImGui_ImplDX9_InvalidateDeviceObjects();
	}

	const HRESULT result = oReset(device, presentation_parameters);
	if (SUCCEEDED(result) && can_use_imgui && !global::shutdown.load(std::memory_order_acquire))
	{
		ImGui_ImplDX9_CreateDeviceObjects();
	}

	return result;
}

HRESULT WINAPI hkPresent(IDirect3DDevice9* device, const RECT* source_rect, const RECT* destination_rect,
	HWND destination_window, const RGNDATA* dirty_region)
{
	if (oPresent == nullptr || device == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	render_frame(device, frame_callback::present);
	return oPresent(device, source_rect, destination_rect, destination_window, dirty_region);
}

HRESULT WINAPI hkEndScene(IDirect3DDevice9* device)
{
	if (oEndScene == nullptr || device == nullptr)
	{
		return D3DERR_INVALIDCALL;
	}

	render_frame(device, frame_callback::end_scene);
	return oEndScene(device);
}

}

bool impl::d3d9::init()
{
	const HMODULE game_module = GetModuleHandleA(nullptr);
	if (game_module == nullptr)
	{
		logger::log_error("D3D9 renderer preflight failed: game module is unavailable");
		return false;
	}

	void** direct3d_create9_slot = find_import_slot(game_module, "d3d9.dll", "Direct3DCreate9");
	if (direct3d_create9_slot != nullptr)
	{
		void* original = nullptr;
		original = *direct3d_create9_slot;
		direct3d_create9_provider.store(reinterpret_cast<direct3d_create9>(original), std::memory_order_release);
		if (!patch_import_slot(direct3d_create9_slot, reinterpret_cast<void*>(&hkDirect3DCreate9CallSite), &original))
		{
			direct3d_create9_provider.store(nullptr, std::memory_order_release);
			logger::log_error("D3D9 SPEED2.EXE Direct3DCreate9 import hook could not be installed");
			return false;
		}

		direct3d_create9_import_slot = direct3d_create9_slot;
		direct3d_create9_provider.store(reinterpret_cast<direct3d_create9>(original), std::memory_order_release);
		logger::log_info("D3D9 SPEED2.EXE Direct3DCreate9 import call-site hook armed; provider export untouched");
		return true;
	}

	// ponytail: use the game's imported resolver only when a direct import is absent; no provider call from the worker.
	void** get_proc_address_slot = find_import_slot(game_module, nullptr, "GetProcAddress");
	if (get_proc_address_slot == nullptr)
	{
		logger::log_error("D3D9 renderer preflight failed: SPEED2.EXE has no Direct3DCreate9 or GetProcAddress import");
		return false;
	}

	void* original = nullptr;
	original = *get_proc_address_slot;
	oGetProcAddress = reinterpret_cast<get_proc_address>(original);
	if (!patch_import_slot(get_proc_address_slot, reinterpret_cast<void*>(&hkGetProcAddress), &original))
	{
		oGetProcAddress = nullptr;
		logger::log_error("D3D9 SPEED2.EXE GetProcAddress import hook could not be installed");
		return false;
	}

	get_proc_address_import_slot = get_proc_address_slot;
	oGetProcAddress = reinterpret_cast<get_proc_address>(original);
	logger::log_info("D3D9 Direct3DCreate9 import absent; SPEED2.EXE GetProcAddress call-site hook armed; provider export untouched");

	return true;
}

bool impl::d3d9::has_call_site_callback()
{
	return direct3d_create_call_site_seen.load(std::memory_order_acquire);
}

bool impl::d3d9::has_factory_callback()
{
	return factory_callback_seen.load(std::memory_order_acquire);
}

bool impl::d3d9::has_create_device_callback()
{
	return create_device_callback_seen.load(std::memory_order_acquire);
}

void impl::d3d9::cleanup()
{
	std::lock_guard<std::mutex> lock(hook_mutex);
	remove_hook(end_scene_target);
	remove_hook(present_target);
	remove_hook(reset_target);
	remove_hook(create_device_target);
	restore_import_slot(get_proc_address_import_slot, reinterpret_cast<void*>(&hkGetProcAddress),
		reinterpret_cast<void*>(oGetProcAddress));
	restore_import_slot(direct3d_create9_import_slot, reinterpret_cast<void*>(&hkDirect3DCreate9CallSite),
		reinterpret_cast<void*>(direct3d_create9_provider.load(std::memory_order_acquire)));

	oEndScene = nullptr;
	oPresent = nullptr;
	oReset = nullptr;
	oCreateDevice = nullptr;
	oGetProcAddress = nullptr;
	direct3d_create9_provider.store(nullptr, std::memory_order_release);
	get_proc_address_import_slot = nullptr;
	direct3d_create9_import_slot = nullptr;
	device_hooks_bound.store(false, std::memory_order_release);
	create_device_hook_attempted = false;
	device_hook_attempted = false;
}

#endif // KIERO_INCLUDE_D3D9
