#if KIERO_INCLUDE_D3D9

#include "d3d9_impl.h"
#include <d3d9.h>
#include "logger/logger.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>

namespace
{
	using direct3d_create9 = IDirect3D9*(WINAPI*)(UINT);
	using create_device = HRESULT(WINAPI*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
	using reset = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
	using present = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
	using end_scene = HRESULT(WINAPI*)(IDirect3DDevice9*);

	constexpr std::uint16_t create_device_index = 16;
	constexpr std::uint16_t reset_index = 16;
	constexpr std::uint16_t present_index = 17;
	constexpr std::uint16_t end_scene_index = 42;

	direct3d_create9 oDirect3DCreate9 = nullptr;
	create_device oCreateDevice = nullptr;
	reset oReset = nullptr;
	present oPresent = nullptr;
	end_scene oEndScene = nullptr;

	std::mutex hook_mutex;
	std::mutex runtime_mutex;
	bool create_device_hook_attempted = false;
	bool device_hook_attempted = false;
	std::atomic_bool device_hooks_bound{ false };
	std::atomic_bool imgui_initialized{ false };
	std::atomic_bool direct3d_create_logged{ false };
	std::atomic_bool frame_callback_logged{ false };

	enum class frame_callback : std::uint8_t
	{
		none,
		end_scene,
		present,
	};

	std::atomic<frame_callback> selected_frame_callback{ frame_callback::none };

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

	IDirect3D9* call_original_create9(bool* faulted)
	{
		if (faulted != nullptr)
		{
			*faulted = false;
		}
		if (oDirect3DCreate9 == nullptr)
		{
			return nullptr;
		}

		__try
		{
			return oDirect3DCreate9(D3D_SDK_VERSION);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			if (faulted != nullptr)
			{
				*faulted = true;
			}
			return nullptr;
		}
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

		void* reset_target = nullptr;
		void* present_target = nullptr;
		void* end_scene_target = nullptr;
		if (!read_vtable_entry(device, reset_index, &reset_target) ||
			!read_vtable_entry(device, present_index, &present_target) ||
			!read_vtable_entry(device, end_scene_index, &end_scene_target))
		{
			logger::log_error("D3D9 live device vtable entries are unavailable; renderer disabled");
			return false;
		}

		const MH_STATUS reset_status = MH_CreateHook(reset_target, reinterpret_cast<LPVOID>(&hkReset), reinterpret_cast<LPVOID*>(&oReset));
		if (reset_status != MH_OK)
		{
			logger::log_error(logger::va("D3D9 live Reset hook failed (status %d)", static_cast<int>(reset_status)));
			return false;
		}

		const MH_STATUS present_status = MH_CreateHook(present_target, reinterpret_cast<LPVOID>(&hkPresent), reinterpret_cast<LPVOID*>(&oPresent));
		if (present_status != MH_OK)
		{
			MH_RemoveHook(reset_target);
			oReset = nullptr;
			logger::log_error(logger::va("D3D9 live Present hook failed (status %d)", static_cast<int>(present_status)));
			return false;
		}

		const MH_STATUS end_scene_status = MH_CreateHook(end_scene_target, reinterpret_cast<LPVOID>(&hkEndScene), reinterpret_cast<LPVOID*>(&oEndScene));
		if (end_scene_status != MH_OK)
		{
			MH_RemoveHook(reset_target);
			MH_RemoveHook(present_target);
			oReset = nullptr;
			oPresent = nullptr;
			logger::log_error(logger::va("D3D9 live EndScene hook failed (status %d)", static_cast<int>(end_scene_status)));
			return false;
		}

		if (!enable_hook(reset_target) || !enable_hook(present_target) || !enable_hook(end_scene_target))
		{
			MH_RemoveHook(reset_target);
			MH_RemoveHook(present_target);
			MH_RemoveHook(end_scene_target);
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
		if (!enable_hook(target))
		{
			MH_RemoveHook(target);
			logger::log_error("D3D9 live CreateDevice hook could not be enabled");
			oCreateDevice = nullptr;
			return false;
		}

		logger::log_debug("D3D9 live CreateDevice hook bound; waiting for the game device");
		return true;
	}

	HRESULT WINAPI hkCreateDevice(IDirect3D9* direct3d9, UINT adapter, D3DDEVTYPE device_type, HWND focus_window,
		DWORD behavior_flags, D3DPRESENT_PARAMETERS* presentation_parameters, IDirect3DDevice9** returned_device)
	{
		if (oCreateDevice == nullptr)
		{
			return D3DERR_INVALIDCALL;
		}

		const HRESULT result = oCreateDevice(direct3d9, adapter, device_type, focus_window, behavior_flags,
			presentation_parameters, returned_device);
		if (SUCCEEDED(result))
		{
			if (returned_device == nullptr || *returned_device == nullptr)
			{
				logger::log_error("D3D9 CreateDevice returned no COM device; renderer disabled");
			}
			else if (!global::shutdown.load(std::memory_order_acquire) && !bind_live_device(*returned_device))
			{
				logger::log_error("D3D9 live device capture failed; renderer disabled");
			}
		}

		return result;
	}

	IDirect3D9* WINAPI hkDirect3DCreate9(UINT sdk_version)
	{
		if (oDirect3DCreate9 == nullptr)
		{
			return nullptr;
		}

		IDirect3D9* direct3d9 = oDirect3DCreate9(sdk_version);
		if (direct3d9 != nullptr && !global::shutdown.load(std::memory_order_acquire))
		{
			if (!direct3d_create_logged.exchange(true, std::memory_order_acq_rel))
			{
				logger::log_debug("D3D9 live Direct3DCreate9 callback captured");
			}
			bind_create_device(direct3d9);
		}

		return direct3d9;
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
	const HMODULE d3d9_module = GetModuleHandleA("d3d9.dll");
	if (d3d9_module == nullptr)
	{
		logger::log_error("D3D9 renderer preflight failed: d3d9.dll is not loaded");
		return false;
	}

	const FARPROC target = GetProcAddress(d3d9_module, "Direct3DCreate9");
	if (target == nullptr)
	{
		logger::log_error("D3D9 renderer preflight failed: Direct3DCreate9 is unavailable");
		return false;
	}

	const MH_STATUS status = MH_CreateHook(reinterpret_cast<LPVOID>(target), reinterpret_cast<LPVOID>(&hkDirect3DCreate9), reinterpret_cast<LPVOID*>(&oDirect3DCreate9));
	if (status != MH_OK)
	{
		logger::log_error(logger::va("D3D9 live Direct3DCreate9 hook failed (status %d)", static_cast<int>(status)));
		return false;
	}

	if (!enable_hook(reinterpret_cast<LPVOID>(target)))
	{
		MH_RemoveHook(reinterpret_cast<LPVOID>(target));
		oDirect3DCreate9 = nullptr;
		logger::log_error("D3D9 live Direct3DCreate9 hook could not be enabled");
		return false;
	}

	// Bind the shared factory method too when the game created its factory before ECM-R.
	bool existing_factory_faulted = false;
	IDirect3D9* existing_factory = call_original_create9(&existing_factory_faulted);
	if (existing_factory_faulted)
	{
		logger::log_error("D3D9 live factory probe faulted; renderer disabled");
		MH_RemoveHook(reinterpret_cast<LPVOID>(target));
		oDirect3DCreate9 = nullptr;
		return false;
	}

	if (existing_factory != nullptr)
	{
		const bool factory_hooked = bind_create_device(existing_factory);
		existing_factory->Release();
		if (!factory_hooked)
		{
			logger::log_error("D3D9 live factory hook was not usable; renderer disabled");
		}
	}

	logger::log_debug("D3D9 live-device backend bound; waiting for the game-created device");
	return true;
}

#endif // KIERO_INCLUDE_D3D9
