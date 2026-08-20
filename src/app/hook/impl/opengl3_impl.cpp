#if KIERO_INCLUDE_OPENGL

#include "logger/logger.hpp"
#include "opengl3_impl.h"

typedef bool(__stdcall* twglSwapBuffers) (_In_ HDC hDc);
twglSwapBuffers owglSwapBuffers;

BOOL __stdcall hkWglSwapBuffers(_In_ HDC hDc)
{
	static bool init = false;

	if (!init)
	{
		global::renderer = kiero::RenderType::Enum::OpenGL;

		HWND hwnd = WindowFromDC(hDc);

		global::hwnd = hwnd;
		logger::log_info("OpenGL first-frame runtime initialization");
		audio::init();
		input::init_overlay();

		menus::init();

		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplOpenGL3_Init();

		init = true;
	}

   input::update();
	menus::prepare();
	menus::update();
	menus::present();

	return owglSwapBuffers(hDc);
}

void impl::opengl3::init()
{
	if (kiero::bind(336, (void**)&owglSwapBuffers, hkWglSwapBuffers) != kiero::Status::Success)
	{
		logger::log_error("Failed to bind OpenGL hook");
		MessageBoxA(nullptr, "Failed to hook OpenGL!", "ECM", 0);
	}
	else
	{
		logger::log_debug("OpenGL hook bound");
	}
}

#endif // KIERO_INCLUDE_OPENGL
