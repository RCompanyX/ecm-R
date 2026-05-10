#include "global.hpp"
#include "logger/logger.hpp"
#include "fs/fs.hpp"
#include "menus.hpp"
#include "audio/audio.hpp"
#include "hook/hook.hpp"
#include "input/input.hpp"
#include "settings/settings.hpp"
#include "audio/player.hpp"

#include <array>
#include <cfloat>
#include <shellapi.h>

namespace
{
	constexpr auto kRepositoryUrl = "https://github.com/RCompanyX/ecm-R";
	constexpr auto kIssuesUrl = "https://github.com/RCompanyX/ecm-R/issues";
	input::hotkey_action hotkey_menu_feedback_action = input::hotkey_action::count;
	std::string hotkey_menu_feedback_message;
	bool hotkey_menu_feedback_is_error = false;

	void clear_hotkey_menu_feedback()
	{
		hotkey_menu_feedback_action = input::hotkey_action::count;
		hotkey_menu_feedback_message.clear();
		hotkey_menu_feedback_is_error = false;
	}

	void set_hotkey_menu_feedback(const input::hotkey_action action, const std::string& message, const bool is_error)
	{
		hotkey_menu_feedback_action = action;
		hotkey_menu_feedback_message = message;
		hotkey_menu_feedback_is_error = is_error;
	}

	bool apply_hotkey_change(const input::hotkey_binding& binding, const std::uint32_t key, std::string& error_message)
	{
		const std::uint32_t previous_key = *binding.runtime_key;
		if (!input::assign_hotkey(binding.action, key, &error_message))
		{
			return false;
		}

		if (!settings::save_hotkey_binding(binding.ini_key, *binding.runtime_key))
		{
			input::assign_hotkey(binding.action, previous_key);
			error_message = "Failed to save the hotkey in the INI file.";
			return false;
		}

		return true;
	}

	bool reset_all_hotkeys_with_persistence(std::string& error_message)
	{
		std::array<std::uint32_t, input::hotkey_count> previous_keys{};
		std::size_t index = 0;
		for (const auto& binding : input::hotkey_bindings())
		{
			previous_keys[index++] = *binding.runtime_key;
		}

		input::reset_all_hotkeys();
		if (settings::save_all_hotkey_bindings())
		{
			return true;
		}

		index = 0;
		for (const auto& binding : input::hotkey_bindings())
		{
			input::assign_hotkey(binding.action, previous_keys[index++]);
		}

		error_message = "Failed to save the default hotkeys in the INI file.";
		return false;
	}

	const char* hotkey_label_for_action(const input::hotkey_action action)
	{
		for (const auto& binding : input::hotkey_bindings())
		{
			if (binding.action == action)
			{
				return binding.label;
			}
		}

		return nullptr;
	}

	void open_external_link(const char* url)
	{
		const auto result = reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL));
		if (result <= 32)
		{
			global::msg_box("ECM-R", std::string("Failed to open link:\n") + url);
		}
	}
}

void menus::init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::GetIO().IniFilename = nullptr;

	menus::build_font(ImGui::GetIO());
}

void menus::prepare()
{
	switch (global::renderer)
	{
	case kiero::RenderType::D3D9:
		ImGui_ImplDX9_NewFrame();
		break;

	case kiero::RenderType::D3D10:
		ImGui_ImplDX10_NewFrame();
		break;

	case kiero::RenderType::D3D11:
		ImGui_ImplDX11_NewFrame();
		break;

	case kiero::RenderType::OpenGL:
		ImGui_ImplOpenGL3_NewFrame();
		break;
	}

	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void menus::present()
{
	ImGui::EndFrame();
	ImGui::Render();

	switch (global::renderer)
	{
	case kiero::RenderType::D3D9:
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		break;

	case kiero::RenderType::D3D10:
		ImGui_ImplDX10_RenderDrawData(ImGui::GetDrawData());
		break;

	case kiero::RenderType::D3D11:
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		break;

	case kiero::RenderType::OpenGL:
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		break;
	}
}

void menus::update()
{
	ImGui::GetIO().MouseDrawCursor = !global::hide;

	if (!global::hide)
	{
		ImGui::SetNextWindowPos({ 0, 0 });
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
		if (ImGui::Begin("ECM", nullptr, flags))
		{
			menus::main_menu_bar();
			ImGui::End();
		}
	}
}

void menus::main_menu_bar()
{
	if (ImGui::BeginMainMenuBar())
	{
		menus::actions();
		menus::hotkeys();
		menus::playlist();

		ImGui::Text(logger::va("Listening: %s on %s", audio::currently_playing.title.c_str(), audio::playlist_name.c_str()).c_str());
		ImGui::SameLine();
		ImGui::Text("[%s]", audio::manual_paused ? "Paused" : "Playing");

		const ImGuiStyle& style = ImGui::GetStyle();
		const float about_width = ImGui::CalcTextSize("About").x + style.FramePadding.x * 2.0f;
     const float available_width = ImGui::GetContentRegionAvail().x;
		const float spacer_width = available_width - about_width;
		if (spacer_width > 0.0f)
		{
         ImGui::Dummy(ImVec2(spacer_width, 0.0f));
			ImGui::SameLine();
		}

		menus::about();

		ImGui::EndMainMenuBar();
	}
}

void menus::actions()
{
	if (ImGui::BeginMenu("Actions"))
	{
		ImGui::Text("Audio Controls");
        ImGui::PushItemWidth(120.0f);

		auto save_volume_setting = [](const char* key, const int value)
		{
			settings::save_core_integer(key, value);
		};

		auto draw_volume_slider = [&](const char* label, std::int32_t& value, const char* config_key)
		{
			if (ImGui::SliderInt(label, &value, 0, 100))
			{
				audio::apply_current_context_volume();
				save_volume_setting(config_key, value);
			}
		};

		const std::string current_context = audio::current_playlist_context();
		const bool is_frontend_context = current_context == "Frontend";
		const bool is_ingame_context = current_context == "In-game";

       if (is_ingame_context)
		{
			draw_volume_slider("Current Volume (In-game)", audio::ingame_volume, "ingame_volume");
			draw_volume_slider("Frontend Volume", audio::frontend_volume, "frontend_volume");
		}
		else
		{
			const char* current_label = is_frontend_context ? "Current Volume (Frontend)" : "Frontend Volume";
			draw_volume_slider(current_label, audio::frontend_volume, "frontend_volume");
			draw_volume_slider("In-game Volume", audio::ingame_volume, "ingame_volume");
		}

		if (ImGui::Button(audio::manual_paused ? "Resume" : "Pause"))
		{
			audio::toggle_manual_pause();
		}

		ImGui::SameLine();

        if (ImGui::Button("Previous"))
		{
			audio::play_previous_song();
		}

		ImGui::SameLine();

		if (ImGui::Button("Skip"))
		{
			audio::skip_to_next_track();
		}

		bool shuffle_enabled = audio::shuffle_enabled;
		if (ImGui::Checkbox("Shuffle", &shuffle_enabled))
		{
			audio::set_shuffle_enabled(shuffle_enabled);
		}

		bool repeat_enabled = audio::repeat_enabled;
		if (ImGui::Checkbox("Repeat", &repeat_enabled))
		{
			audio::set_repeat_enabled(repeat_enabled);
		}

		ImGui::Text("Mode: %s", audio::shuffle_enabled ? "Random" : "Sequential");
     ImGui::Text("Repeat: %s", audio::repeat_enabled ? "All" : "Off");
      ImGui::Text("Manual Pause: %s", audio::manual_paused ? "On" : "Off");
		ImGui::Text("Context: %s", audio::current_playlist_context());
		ImGui::Text("Active Volume: %d", audio::current_context_volume());
		ImGui::Text("Tracks: %d", audio::current_playlist_track_count());

		ImGui::EndMenu();
	}
}

void menus::hotkeys()
{
	if (!ImGui::BeginMenu("Hotkeys"))
	{
		return;
	}

	const bool capture_active = input::is_hotkey_capture_active();
	const input::hotkey_action capture_action = input::captured_hotkey_action();
	const input::hotkey_action capture_feedback_action = input::capture_feedback_action();
	const char* capture_feedback_message = input::capture_feedback_message();
	const bool has_capture_feedback = capture_feedback_message && capture_feedback_message[0] != '\0';
	const char* capture_label = capture_active ? hotkey_label_for_action(capture_action) : nullptr;
	const ImVec4 error_color(0.90f, 0.35f, 0.35f, 1.0f);
	const ImVec4 success_color(0.40f, 0.78f, 0.40f, 1.0f);

	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f);
	if (audio::are_hotkeys_locked())
	{
		ImGui::TextWrapped("ECM-R hotkeys stay locked until the first startup chyron has appeared and disappeared once.");
	}
	else
	{
		ImGui::TextWrapped("ECM-R hotkeys are ready. Supported keys: %s.", input::supported_key_help());
	}
	ImGui::TextWrapped("While capture is active, ECM-R suspends hotkey execution so the candidate key does not trigger playback, overlay, shuffle, or repeat actions.");
	if (capture_label)
	{
		ImGui::TextWrapped("Capturing binding for: %s", capture_label);
	}
	ImGui::PopTextWrapPos();
	ImGui::Spacing();

	for (const auto& binding : input::hotkey_bindings())
	{
		ImGui::PushID(static_cast<int>(binding.action));
		const bool capturing_this = capture_active && capture_action == binding.action;
		const std::string current_key = input::key_to_string(*binding.runtime_key);
		const bool show_capture_feedback = capture_feedback_action == binding.action && has_capture_feedback;
		const bool show_menu_feedback = hotkey_menu_feedback_action == binding.action && !hotkey_menu_feedback_message.empty();

		ImGui::Separator();
		ImGui::Text("%s", binding.label);
		ImGui::SameLine(230.0f);
		ImGui::Text("%s", current_key.c_str());

		if (capturing_this)
		{
			ImGui::TextWrapped("Press a supported key to bind this action.");
			if (ImGui::Button("Cancel"))
			{
				input::cancel_hotkey_capture();
			}
		}
		else if (!capture_active)
		{
			if (ImGui::Button("Rebind"))
			{
				clear_hotkey_menu_feedback();
				input::begin_hotkey_capture(binding.action);
			}

			if (binding.action != input::hotkey_action::toggle_overlay)
			{
				ImGui::SameLine();
				if (ImGui::Button("Clear"))
				{
					std::string error_message;
					input::clear_capture_feedback();
					if (apply_hotkey_change(binding, input::unbound_key, error_message))
					{
						set_hotkey_menu_feedback(binding.action, "Binding cleared.", false);
					}
					else
					{
						set_hotkey_menu_feedback(binding.action, error_message, true);
					}
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Reset"))
			{
				std::string error_message;
				input::clear_capture_feedback();
				if (apply_hotkey_change(binding, binding.default_key, error_message))
				{
					set_hotkey_menu_feedback(binding.action, std::string("Reset to ") + input::key_to_string(binding.default_key) + ".", false);
				}
				else
				{
					set_hotkey_menu_feedback(binding.action, error_message, true);
				}
			}
		}
		else
		{
			ImGui::TextDisabled("Capture in progress...");
		}

		if (binding.action == input::hotkey_action::toggle_overlay && *binding.runtime_key == input::unbound_key)
		{
			ImGui::TextColored(error_color, "Overlay is currently unbound. Rebind it before closing this menu.");
		}

		if (show_capture_feedback)
		{
			ImGui::TextColored(input::capture_feedback_is_error() ? error_color : success_color, "%s", capture_feedback_message);
		}
		else if (show_menu_feedback)
		{
			ImGui::TextColored(hotkey_menu_feedback_is_error ? error_color : success_color, "%s", hotkey_menu_feedback_message.c_str());
		}

		ImGui::PopID();
	}

	ImGui::Separator();
	if (!capture_active)
	{
		if (ImGui::Button("Reset All"))
		{
			std::string error_message;
			input::clear_capture_feedback();
			if (reset_all_hotkeys_with_persistence(error_message))
			{
				set_hotkey_menu_feedback(input::hotkey_action::count, "All hotkeys reset to their defaults.", false);
			}
			else
			{
				set_hotkey_menu_feedback(input::hotkey_action::count, error_message, true);
			}
		}
	}
	else
	{
		ImGui::TextDisabled("Finish or cancel the active capture before resetting all bindings.");
	}

	ImGui::SameLine();
	ImGui::TextUnformatted("Shuffle and Repeat start as None by default.");

	if (hotkey_menu_feedback_action == input::hotkey_action::count && !hotkey_menu_feedback_message.empty())
	{
		ImGui::TextColored(hotkey_menu_feedback_is_error ? error_color : success_color, "%s", hotkey_menu_feedback_message.c_str());
	}

	ImGui::EndMenu();
}

void menus::about()
{
  ImGui::SetNextWindowSizeConstraints(ImVec2(220.0f, 0.0f), ImVec2(360.0f, FLT_MAX));
	if (ImGui::BeginMenu("About"))
	{
       ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 320.0f);
		ImGui::Text("ECM-R - External Custom Music Reloaded");
		ImGui::Text("Version: %s", VERSION);
		ImGui::Separator();
		ImGui::TextWrapped("Fork of the original ECM (External Custom Music) project.");
		ImGui::BulletText("Original author: BttrDrgn");
		ImGui::BulletText("Current fork maintainer: RCompanyX");
		ImGui::Spacing();
		ImGui::TextWrapped("Report bugs, request features, or share ideas through GitHub Issues.");
		ImGui::PopTextWrapPos();

		if (ImGui::Button("Repository"))
		{
			open_external_link(kRepositoryUrl);
		}

		ImGui::SameLine();

		if (ImGui::Button("Issues"))
		{
			open_external_link(kIssuesUrl);
		}

		ImGui::EndMenu();
	}
}

void menus::playlist()
{
	if (ImGui::BeginMenu("Playlist"))
	{
		for (int i = 0; i < audio::playlist_files.size(); ++i)
		{
			std::string song = audio::playlist_files[i].first;
			logger::rem_path_info(song, audio::playlist_dir);
			ImGui::Text("%s", song.c_str());
		}

		ImGui::EndMenu();
	}
}

void menus::build_font(ImGuiIO& io)
{
	std::string font = "ecm/fonts/NotoSans-Regular.ttf";
	std::string font_jp = "ecm/fonts/NotoSansJP-Regular.ttf";
	std::string emoji = "ecm/fonts/NotoEmoji-Regular.ttf";

	if (fs::exists(font))
	{
		io.Fonts->AddFontFromFileTTF(&font[0], 18.0f);

		static ImFontConfig cfg;
		static ImWchar emoji_ranges[] = { 0x1, 0x1FFFF, 0 };

		if (fs::exists(emoji))
		{
			cfg.MergeMode = true;
			cfg.OversampleH = cfg.OversampleV = 1;
			//cfg.FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_LoadColor;	//Noto doesnt have color
			io.Fonts->AddFontFromFileTTF(&emoji[0], 12.0f, &cfg, emoji_ranges);
		}

		if (fs::exists(font_jp))
		{
			ImFontConfig cfg;
			cfg.OversampleH = cfg.OversampleV = 1;
			cfg.MergeMode = true;
			io.Fonts->AddFontFromFileTTF(&font_jp[0], 18.0f, &cfg, io.Fonts->GetGlyphRangesJapanese());
		}
	}
}
