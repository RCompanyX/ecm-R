#include "global.hpp"
#include "logger/logger.hpp"
#include "fs/fs.hpp"
#include "menus.hpp"
#include "audio/audio.hpp"
#include "hook/hook.hpp"
#include "input/input.hpp"
#include "settings/settings.hpp"
#include "audio/player.hpp"
#include "localization/localization.hpp"

#include <array>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <future>
#include <mutex>
#include <shellapi.h>
#include <winhttp.h>

namespace
{
	constexpr auto kRepositoryUrl = "https://github.com/RCompanyX/ecm-R";
	constexpr auto kIssuesUrl = "https://github.com/RCompanyX/ecm-R/issues";
	constexpr wchar_t kLatestReleaseHost[] = L"api.github.com";
	constexpr wchar_t kReleaseListPath[] = L"/repos/RCompanyX/ecm-R/releases?per_page=10";
	input::hotkey_action hotkey_menu_feedback_action = input::hotkey_action::count;
	std::string hotkey_menu_feedback_message;
	bool hotkey_menu_feedback_is_error = false;
	bool language_save_failed = false;
	bool pending_language_change = false;
	localization::language pending_language = localization::language::en;
	enum class release_discovery_policy
	{
		latest_published,
		latest_non_draft,
		latest_stable,
	};
	enum class version_check_state
	{
		idle,
		checking,
		up_to_date,
		update_available,
		failed,
	};
	constexpr release_discovery_policy kReleaseDiscoveryPolicy = release_discovery_policy::latest_non_draft;
	std::atomic<version_check_state> version_status = version_check_state::idle;
	std::once_flag version_check_once;
	std::mutex version_mutex;
	std::future<void> version_check_task;

	// Small subset of release metadata extracted from the GitHub releases API.
	struct github_release_info
	{
		std::string tag_name;
		bool draft = false;
		bool prerelease = false;
	};

	github_release_info latest_release_info;

	// Parsed semantic-ish version used for update comparisons.
	struct parsed_version
	{
		std::array<int, 3> numbers{ 0, 0, 0 };
		std::string prerelease;
		bool has_numeric_component = false;
	};

	using winhttp_open_fn = HINTERNET(WINAPI*)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
	using winhttp_connect_fn = HINTERNET(WINAPI*)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
	using winhttp_open_request_fn = HINTERNET(WINAPI*)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
	using winhttp_send_request_fn = BOOL(WINAPI*)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
	using winhttp_receive_response_fn = BOOL(WINAPI*)(HINTERNET, LPVOID);
	using winhttp_query_data_available_fn = BOOL(WINAPI*)(HINTERNET, LPDWORD);
	using winhttp_read_data_fn = BOOL(WINAPI*)(HINTERNET, LPVOID, DWORD, LPDWORD);
	using winhttp_set_timeouts_fn = BOOL(WINAPI*)(HINTERNET, int, int, int, int);
	using winhttp_close_handle_fn = BOOL(WINAPI*)(HINTERNET);

	// Splits a version string into numeric parts and an optional prerelease suffix.
	parsed_version parse_version_string(std::string version)
	{
		version.erase(version.begin(), std::find_if(version.begin(), version.end(), [](const unsigned char ch)
		{
			return !std::isspace(ch);
		}));
		version.erase(std::find_if(version.rbegin(), version.rend(), [](const unsigned char ch)
		{
			return !std::isspace(ch);
		}).base(), version.end());

		if (!version.empty() && (version.front() == 'v' || version.front() == 'V'))
		{
			version.erase(version.begin());
		}

		parsed_version result;
		std::size_t cursor = 0;
		std::size_t numeric_index = 0;
		while (cursor < version.size() && numeric_index < result.numbers.size())
		{
			if (!std::isdigit(static_cast<unsigned char>(version[cursor])))
			{
				break;
			}

			const std::size_t start = cursor;
			while (cursor < version.size() && std::isdigit(static_cast<unsigned char>(version[cursor])))
			{
				++cursor;
			}

			result.numbers[numeric_index++] = std::stoi(version.substr(start, cursor - start));
			result.has_numeric_component = true;

			if (cursor < version.size() && version[cursor] == '.')
			{
				++cursor;
				continue;
			}

			break;
		}

		if (cursor < version.size() && version[cursor] == '-')
		{
			result.prerelease = version.substr(cursor + 1);
		}
		else if (cursor < version.size())
		{
			result.prerelease = version.substr(cursor);
		}

		return result;
	}

	// Compares two ECM-R version strings for release-update detection.
	int compare_versions(const std::string& lhs, const std::string& rhs)
	{
		const parsed_version left = parse_version_string(lhs);
		const parsed_version right = parse_version_string(rhs);

		if (!left.has_numeric_component || !right.has_numeric_component)
		{
			if (lhs == rhs)
			{
				return 0;
			}

			return lhs < rhs ? -1 : 1;
		}

		for (std::size_t i = 0; i < left.numbers.size(); ++i)
		{
			if (left.numbers[i] != right.numbers[i])
			{
				return left.numbers[i] < right.numbers[i] ? -1 : 1;
			}
		}

		if (left.prerelease == right.prerelease)
		{
			return 0;
		}

		if (left.prerelease.empty())
		{
			return 1;
		}

		if (right.prerelease.empty())
		{
			return -1;
		}

		return left.prerelease < right.prerelease ? -1 : 1;
	}

	// Extracts a JSON string field using a minimal parser tailored to the GitHub response.
	bool try_extract_json_string_field(const std::string& source, const char* key, std::string& value)
	{
		const std::size_t key_pos = source.find(key);
		if (key_pos == std::string::npos)
		{
			return false;
		}

		const std::size_t colon_pos = source.find(':', key_pos);
		if (colon_pos == std::string::npos)
		{
			return false;
		}

		const std::size_t value_start = source.find('"', colon_pos + 1);
		if (value_start == std::string::npos)
		{
			return false;
		}

		std::string extracted_value;
		for (std::size_t cursor = value_start + 1; cursor < source.size(); ++cursor)
		{
			const char current = source[cursor];
			if (current == '\\')
			{
				if (cursor + 1 >= source.size())
				{
					return false;
				}

				extracted_value.push_back(source[cursor + 1]);
				++cursor;
				continue;
			}

			if (current == '"')
			{
				value = extracted_value;
				return true;
			}

			extracted_value.push_back(current);
		}

		return false;
	}

	// Extracts a JSON boolean field using a minimal parser tailored to the GitHub response.
	bool try_extract_json_bool_field(const std::string& source, const char* key, bool& value)
	{
		const std::size_t key_pos = source.find(key);
		if (key_pos == std::string::npos)
		{
			return false;
		}

		const std::size_t colon_pos = source.find(':', key_pos);
		if (colon_pos == std::string::npos)
		{
			return false;
		}

		const std::size_t value_pos = source.find_first_not_of(" \t\r\n", colon_pos + 1);
		if (value_pos == std::string::npos)
		{
			return false;
		}

		if (source.compare(value_pos, 4, "true") == 0)
		{
			value = true;
			return true;
		}

		if (source.compare(value_pos, 5, "false") == 0)
		{
			value = false;
			return true;
		}

		return false;
	}

	// Parses the GitHub releases payload into the subset of fields ECM-R cares about.
	std::vector<github_release_info> parse_release_list(const std::string& response_body)
	{
		std::vector<github_release_info> releases;
		std::size_t search_pos = 0;

		while ((search_pos = response_body.find("\"tag_name\"", search_pos)) != std::string::npos)
		{
			const std::size_t body_pos = response_body.find("\"body\"", search_pos);
			if (body_pos == std::string::npos)
			{
				break;
			}

			const std::string release_segment = response_body.substr(search_pos, body_pos - search_pos);
			github_release_info release;
			if (try_extract_json_string_field(release_segment, "\"tag_name\"", release.tag_name) &&
				try_extract_json_bool_field(release_segment, "\"draft\"", release.draft) &&
				try_extract_json_bool_field(release_segment, "\"prerelease\"", release.prerelease))
			{
				releases.push_back(std::move(release));
			}

			search_pos = body_pos;
		}

		return releases;
	}

	// Filters releases according to the configured update-discovery policy.
	bool release_matches_discovery_policy(const github_release_info& release)
	{
		switch (kReleaseDiscoveryPolicy)
		{
		case release_discovery_policy::latest_published:
			return true;

		case release_discovery_policy::latest_non_draft:
			return !release.draft;

		case release_discovery_policy::latest_stable:
			return !release.draft && !release.prerelease;
		}

		return false;
	}

	// Selects the newest release that should be considered for update checks.
	github_release_info extract_latest_release_info(const std::string& response_body)
	{
		for (const github_release_info& release : parse_release_list(response_body))
		{
			if (release_matches_discovery_policy(release))
			{
				return release;
			}
		}

		return {};
	}

	// Returns the menu label shown when a newer release has been found.
	const char* version_update_label(const github_release_info& release)
	{
		return localization::text(release.prerelease ? "release.new_testing" : "release.new_stable");
	}

	// Returns the tooltip label that describes the discovered release channel.
	const char* latest_release_tooltip_label(const github_release_info& release)
	{
		return localization::text(release.prerelease ? "release.latest_testing" : "release.latest_stable");
	}

	// Fetches the GitHub release list through WinHTTP loaded at runtime.
	std::string fetch_release_list_response()
	{
		HMODULE winhttp_module = LoadLibraryW(L"winhttp.dll");
		if (winhttp_module == nullptr)
		{
			return {};
		}

		const auto winhttp_open = reinterpret_cast<winhttp_open_fn>(GetProcAddress(winhttp_module, "WinHttpOpen"));
		const auto winhttp_connect = reinterpret_cast<winhttp_connect_fn>(GetProcAddress(winhttp_module, "WinHttpConnect"));
		const auto winhttp_open_request = reinterpret_cast<winhttp_open_request_fn>(GetProcAddress(winhttp_module, "WinHttpOpenRequest"));
		const auto winhttp_send_request = reinterpret_cast<winhttp_send_request_fn>(GetProcAddress(winhttp_module, "WinHttpSendRequest"));
		const auto winhttp_receive_response = reinterpret_cast<winhttp_receive_response_fn>(GetProcAddress(winhttp_module, "WinHttpReceiveResponse"));
		const auto winhttp_query_data_available = reinterpret_cast<winhttp_query_data_available_fn>(GetProcAddress(winhttp_module, "WinHttpQueryDataAvailable"));
		const auto winhttp_read_data = reinterpret_cast<winhttp_read_data_fn>(GetProcAddress(winhttp_module, "WinHttpReadData"));
		const auto winhttp_set_timeouts = reinterpret_cast<winhttp_set_timeouts_fn>(GetProcAddress(winhttp_module, "WinHttpSetTimeouts"));
		const auto winhttp_close_handle = reinterpret_cast<winhttp_close_handle_fn>(GetProcAddress(winhttp_module, "WinHttpCloseHandle"));
		if (winhttp_open == nullptr || winhttp_connect == nullptr || winhttp_open_request == nullptr ||
			winhttp_send_request == nullptr || winhttp_receive_response == nullptr ||
			winhttp_query_data_available == nullptr || winhttp_read_data == nullptr ||
			winhttp_set_timeouts == nullptr || winhttp_close_handle == nullptr)
		{
			FreeLibrary(winhttp_module);
			return {};
		}

		std::string response_body;
		HINTERNET session = winhttp_open(L"ECM-R Version Check/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		HINTERNET connection = nullptr;
		HINTERNET request = nullptr;

		if (session != nullptr)
		{
			winhttp_set_timeouts(session, 3000, 3000, 3000, 3000);
			connection = winhttp_connect(session, kLatestReleaseHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
		}

		if (connection != nullptr)
		{
			static const wchar_t* accept_types[] = { L"*/*", nullptr };
			request = winhttp_open_request(connection, L"GET", kReleaseListPath, nullptr, WINHTTP_NO_REFERER, accept_types, WINHTTP_FLAG_SECURE);
		}

		if (request != nullptr)
		{
			const wchar_t* headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
			if (winhttp_send_request(request, headers, -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
				winhttp_receive_response(request, nullptr))
			{
				DWORD bytes_available = 0;
				while (winhttp_query_data_available(request, &bytes_available) && bytes_available > 0)
				{
					std::string chunk(bytes_available, '\0');
					DWORD bytes_read = 0;
					if (!winhttp_read_data(request, chunk.data(), bytes_available, &bytes_read))
					{
						response_body.clear();
						break;
					}

					chunk.resize(bytes_read);
					response_body += chunk;
					bytes_available = 0;
				}
			}
		}

		if (request != nullptr)
		{
			winhttp_close_handle(request);
		}

		if (connection != nullptr)
		{
			winhttp_close_handle(connection);
		}

		if (session != nullptr)
		{
			winhttp_close_handle(session);
		}

		FreeLibrary(winhttp_module);
		return response_body;
	}

	// Starts the asynchronous version check once per process lifetime.
	void run_version_check_once()
	{
		version_status.store(version_check_state::checking, std::memory_order_release);
		version_check_task = std::async(std::launch::async, []()
		{
			try
			{
				const github_release_info latest_release = extract_latest_release_info(fetch_release_list_response());
				if (latest_release.tag_name.empty())
				{
					version_status.store(version_check_state::failed, std::memory_order_release);
					return;
				}

				{
					std::scoped_lock lock(version_mutex);
					latest_release_info = latest_release;
				}

				const version_check_state next_status = compare_versions(latest_release.tag_name, VERSION) > 0
					? version_check_state::update_available
					: version_check_state::up_to_date;
				version_status.store(next_status, std::memory_order_release);
			}
			catch (...)
			{
				version_status.store(version_check_state::failed, std::memory_order_release);
			}
		});
	}

	// Waits for the async version check only after it has already completed or failed.
	void finalize_version_check_task()
	{
		if (!version_check_task.valid())
		{
			return;
		}

		if (version_status.load(std::memory_order_acquire) == version_check_state::checking)
		{
			return;
		}

		version_check_task.wait();
	}

	// Reports whether the async update check found a newer release than the current build.
	bool has_newer_release_available()
	{
		return version_status.load(std::memory_order_acquire) == version_check_state::update_available;
	}

	// Returns a thread-safe snapshot of the latest discovered release metadata.
	github_release_info current_latest_release_info()
	{
		std::scoped_lock lock(version_mutex);
		return latest_release_info;
	}

	// Draws the update badge shown in the main menu bar when a new release is available.
	void draw_new_version_badge(const github_release_info& release)
	{
		const ImVec4 update_color(0.92f, 0.25f, 0.25f, 1.0f);
		ImGui::TextColored(update_color, "%s", version_update_label(release));
		if (ImGui::IsItemHovered())
		{
			if (!release.tag_name.empty())
			{
				ImGui::SetTooltip("%s: %s", latest_release_tooltip_label(release), release.tag_name.c_str());
			}
		}
	}

	// Clears menu-local hotkey feedback created by button-based actions.
	void clear_hotkey_menu_feedback()
	{
		hotkey_menu_feedback_action = input::hotkey_action::count;
		hotkey_menu_feedback_message.clear();
		hotkey_menu_feedback_is_error = false;
	}

	// Stores menu-local hotkey feedback after a save, reset, or clear action.
	void set_hotkey_menu_feedback(const input::hotkey_action action, const std::string& message, const bool is_error)
	{
		hotkey_menu_feedback_action = action;
		hotkey_menu_feedback_message = message;
		hotkey_menu_feedback_is_error = is_error;
	}

	// Applies a hotkey change and rolls it back if the INI file cannot be updated.
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
			error_message = localization::text("hotkeys.save_failed");
			return false;
		}

		return true;
	}

	// Resets every hotkey and restores the previous state if persistence fails.
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

		error_message = localization::text("hotkeys.save_failed");
		return false;
	}

	// Resolves the display label for a hotkey action shown in the menu.
	const char* hotkey_label_for_action(const input::hotkey_action action)
	{
		for (const auto& binding : input::hotkey_bindings())
		{
			if (binding.action == action)
			{
				return localization::text(binding.label_key);
			}
		}

		return nullptr;
	}

	void request_language_change(const localization::language value)
	{
		if (value == localization::current())
		{
			return;
		}

		if (!settings::save_language(value))
		{
			language_save_failed = true;
			return;
		}

		language_save_failed = false;
		pending_language = value;
		pending_language_change = true;
	}

	void apply_pending_language_change()
	{
		if (!pending_language_change)
		{
			return;
		}

		localization::set_language(pending_language);
		input::clear_capture_feedback();
		clear_hotkey_menu_feedback();
		pending_language_change = false;
	}

	// Opens an external URL and reports a failure if Windows cannot launch it.
	void open_external_link(const char* url)
	{
		const auto result = reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL));
		if (result <= 32)
		{
			global::msg_box("ECM-R", localization::format("about.open_failed", {{ "url", url }}));
		}
	}
}

// Creates the ImGui context, loads fonts, and kicks off the release check.
void menus::init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::GetIO().IniFilename = nullptr;

	menus::build_font(ImGui::GetIO());
	std::call_once(version_check_once, run_version_check_once);
}

// Starts a new ImGui frame using the backend that matches the active renderer.
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

// Finalizes and submits the current ImGui frame through the active renderer backend.
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

// Updates overlay visibility and draws the top-level ECM-R menu bar when shown.
void menus::update()
{
	finalize_version_check_task();
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

	apply_pending_language_change();
}

// Renders the main menu bar with playback status, update badge, and submenus.
void menus::main_menu_bar()
{
	if (ImGui::BeginMainMenuBar())
	{
		menus::actions();
		menus::hotkeys();
		menus::playlist();
		menus::language_menu();

		const std::string display_name = (audio::currently_playing.artist != "N/A")
			? audio::currently_playing.artist + " - " + audio::currently_playing.title
			: audio::currently_playing.title;
		const std::string listening = localization::format("status.listening", {{ "track", display_name }, { "playlist", audio::playlist_name }});
		ImGui::Text("%s", listening.c_str());
		ImGui::SameLine();
		ImGui::Text("[%s]", localization::text(audio::manual_paused ? "status.paused" : "status.playing"));

		const ImGuiStyle& style = ImGui::GetStyle();
		const float about_width = ImGui::CalcTextSize(localization::text("menu.about")).x + style.FramePadding.x * 2.0f;
		const github_release_info latest_release = current_latest_release_info();
		const bool show_version_update = has_newer_release_available() && !latest_release.tag_name.empty();
		const float version_update_width = show_version_update ? ImGui::CalcTextSize(version_update_label(latest_release)).x + style.ItemSpacing.x : 0.0f;
		const float available_width = ImGui::GetContentRegionAvail().x;
		const float spacer_width = available_width - about_width - version_update_width;
		if (spacer_width > 0.0f)
		{
			ImGui::Dummy(ImVec2(spacer_width, 0.0f));
			ImGui::SameLine();
		}

		if (show_version_update)
		{
			draw_new_version_badge(latest_release);
			ImGui::SameLine();
		}

		menus::about();

		ImGui::EndMainMenuBar();
	}
}

// Renders the language selector; the active bundle changes after this frame is drawn.
void menus::language_menu()
{
	const std::string menu_label = std::string(localization::text("menu.language")) + "##language_menu";
	if (!ImGui::BeginMenu(menu_label.c_str()))
	{
		return;
	}

	const localization::language active = localization::current();
	const std::string english_label = std::string(localization::text("language.english")) + "##language_en";
	if (ImGui::MenuItem(english_label.c_str(), nullptr, active == localization::language::en))
	{
		request_language_change(localization::language::en);
	}

	const std::string spanish_label = std::string(localization::text("language.spanish")) + "##language_es";
	if (ImGui::MenuItem(spanish_label.c_str(), nullptr, active == localization::language::es))
	{
		request_language_change(localization::language::es);
	}

	if (language_save_failed)
	{
		ImGui::TextWrapped("%s", localization::text("language.save_failed"));
	}

	ImGui::EndMenu();
}

// Placeholder for future experimental controls; not invoked by the current menu bar.
void menus::experimental()
{
}

// Renders playback controls, context-aware volume sliders, and runtime status.
void menus::actions()
{
	const std::string menu_label = std::string(localization::text("menu.actions")) + "##actions_menu";
	if (ImGui::BeginMenu(menu_label.c_str()))
	{
		ImGui::Text("%s", localization::text("actions.audio_controls"));
        ImGui::PushItemWidth(120.0f);

		auto save_volume_setting = [](const char* key, const int value)
		{
			settings::save_core_integer(key, value);
		};

		auto draw_volume_slider = [&](const char* text_key, std::int32_t& value, const char* config_key, const char* widget_id)
		{
			const std::string label = std::string(localization::text(text_key)) + "##" + widget_id;
			if (ImGui::SliderInt(label.c_str(), &value, 0, 100))
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
			draw_volume_slider("actions.current_volume_ingame", audio::ingame_volume, "ingame_volume", "current_ingame_volume");
			draw_volume_slider("actions.frontend_volume", audio::frontend_volume, "frontend_volume", "frontend_volume");
		}
		else
		{
			const char* current_label = is_frontend_context ? "actions.current_volume_frontend" : "actions.frontend_volume";
			draw_volume_slider(current_label, audio::frontend_volume, "frontend_volume", "frontend_volume");
			draw_volume_slider("actions.ingame_volume", audio::ingame_volume, "ingame_volume", "ingame_volume");
		}

		const std::string pause_label = std::string(localization::text(audio::manual_paused ? "actions.resume" : "actions.pause")) + "##pause";
		if (ImGui::Button(pause_label.c_str()))
		{
			audio::toggle_manual_pause();
		}

		ImGui::SameLine();

		const std::string previous_label = std::string(localization::text("actions.previous")) + "##previous";
		if (ImGui::Button(previous_label.c_str()))
		{
			audio::play_previous_song();
		}

		ImGui::SameLine();

		const std::string skip_label = std::string(localization::text("actions.skip")) + "##skip";
		if (ImGui::Button(skip_label.c_str()))
		{
			audio::skip_to_next_track();
		}

		bool shuffle_enabled = audio::shuffle_enabled;
		const std::string shuffle_label = std::string(localization::text("actions.shuffle")) + "##shuffle";
		if (ImGui::Checkbox(shuffle_label.c_str(), &shuffle_enabled))
		{
			audio::set_shuffle_enabled(shuffle_enabled);
		}

		bool repeat_enabled = audio::repeat_enabled;
		const std::string repeat_label = std::string(localization::text("actions.repeat")) + "##repeat";
		if (ImGui::Checkbox(repeat_label.c_str(), &repeat_enabled))
		{
			audio::set_repeat_enabled(repeat_enabled);
		}

		ImGui::Separator();
		bool ingame_movie_muting = audio::ingame_movie_muting;
		const std::string movie_muting_label = std::string(localization::text("actions.ingame_movie_muting")) + "##ingame_movie_muting";
		if (ImGui::Checkbox(movie_muting_label.c_str(), &ingame_movie_muting))
		{
			audio::set_ingame_movie_muting(ingame_movie_muting);
		}
		ImGui::Separator();

		ImGui::Text("%s", localization::format("actions.mode", {{ "mode", localization::text(audio::shuffle_enabled ? "actions.random" : "actions.sequential") }}).c_str());
		ImGui::Text("%s", localization::format("actions.repeat_status", {{ "repeat", localization::text(audio::repeat_enabled ? "actions.all" : "actions.off") }}).c_str());
		ImGui::Text("%s", localization::format("actions.manual_pause", {{ "state", localization::text(audio::manual_paused ? "actions.on" : "actions.off") }}).c_str());
		const char* context_key = is_frontend_context ? "context.frontend" : is_ingame_context ? "context.ingame" : "context.all";
		ImGui::Text("%s", localization::format("actions.context", {{ "context", localization::text(context_key) }}).c_str());
		ImGui::Text("%s", localization::format("actions.active_volume", {{ "volume", std::to_string(audio::current_context_volume()) }}).c_str());
		ImGui::Text("%s", localization::format("actions.tracks", {{ "count", std::to_string(audio::current_playlist_track_count()) }}).c_str());

		ImGui::EndMenu();
	}
}

// Renders hotkey rebinding controls together with capture and persistence feedback.
void menus::hotkeys()
{
	const std::string menu_label = std::string(localization::text("menu.hotkeys")) + "##hotkeys_menu";
	if (!ImGui::BeginMenu(menu_label.c_str()))
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
		ImGui::TextWrapped("%s", localization::text("hotkeys.locked"));
	}
	else
	{
		ImGui::TextWrapped("%s", localization::format("hotkeys.ready", {{ "keys", input::supported_key_help() }}).c_str());
	}
	ImGui::TextWrapped("%s", localization::text("hotkeys.capture_suspended"));
	if (capture_label)
	{
		ImGui::TextWrapped("%s", localization::format("hotkeys.capturing", {{ "action", capture_label }}).c_str());
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
		ImGui::Text("%s", localization::text(binding.label_key));
		ImGui::SameLine(230.0f);
		ImGui::Text("%s", current_key.c_str());

		if (capturing_this)
		{
			ImGui::TextWrapped("%s", localization::text("hotkeys.press_supported"));
			const std::string cancel_label = std::string(localization::text("hotkeys.cancel")) + "##cancel_capture";
			if (ImGui::Button(cancel_label.c_str()))
			{
				input::cancel_hotkey_capture();
			}
		}
		else if (!capture_active)
		{
			const std::string rebind_label = std::string(localization::text("hotkeys.rebind")) + "##rebind";
			if (ImGui::Button(rebind_label.c_str()))
			{
				clear_hotkey_menu_feedback();
				input::begin_hotkey_capture(binding.action);
			}

			if (binding.action != input::hotkey_action::toggle_overlay)
			{
				ImGui::SameLine();
				const std::string clear_label = std::string(localization::text("hotkeys.clear")) + "##clear";
				if (ImGui::Button(clear_label.c_str()))
				{
					std::string error_message;
					input::clear_capture_feedback();
					if (apply_hotkey_change(binding, input::unbound_key, error_message))
					{
						set_hotkey_menu_feedback(binding.action, localization::text("hotkeys.binding_cleared"), false);
					}
					else
					{
						set_hotkey_menu_feedback(binding.action, error_message, true);
					}
				}
			}

			ImGui::SameLine();
			const std::string reset_label = std::string(localization::text("hotkeys.reset")) + "##reset";
			if (ImGui::Button(reset_label.c_str()))
			{
				std::string error_message;
				input::clear_capture_feedback();
				if (apply_hotkey_change(binding, binding.default_key, error_message))
				{
					set_hotkey_menu_feedback(binding.action, localization::format("hotkeys.reset_to", {{ "key", input::key_to_string(binding.default_key) }}), false);
				}
				else
				{
					set_hotkey_menu_feedback(binding.action, error_message, true);
				}
			}
		}
		else
		{
			ImGui::TextDisabled("%s", localization::text("hotkeys.capture_progress"));
		}

		if (binding.action == input::hotkey_action::toggle_overlay && *binding.runtime_key == input::unbound_key)
		{
			ImGui::TextColored(error_color, "%s", localization::text("hotkeys.overlay_unbound"));
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
		const std::string reset_all_label = std::string(localization::text("hotkeys.reset_all")) + "##reset_all";
		if (ImGui::Button(reset_all_label.c_str()))
		{
			std::string error_message;
			input::clear_capture_feedback();
			if (reset_all_hotkeys_with_persistence(error_message))
			{
				set_hotkey_menu_feedback(input::hotkey_action::count, localization::text("hotkeys.all_reset"), false);
			}
			else
			{
				set_hotkey_menu_feedback(input::hotkey_action::count, error_message, true);
			}
		}
	}
	else
	{
		ImGui::TextDisabled("%s", localization::text("hotkeys.finish_capture"));
	}

	ImGui::SameLine();
	ImGui::TextUnformatted(localization::text("hotkeys.shuffle_repeat_none"));

	if (hotkey_menu_feedback_action == input::hotkey_action::count && !hotkey_menu_feedback_message.empty())
	{
		ImGui::TextColored(hotkey_menu_feedback_is_error ? error_color : success_color, "%s", hotkey_menu_feedback_message.c_str());
	}

	ImGui::EndMenu();
}

// Renders project credits, version information, and GitHub entry points.
void menus::about()
{
  ImGui::SetNextWindowSizeConstraints(ImVec2(220.0f, 0.0f), ImVec2(360.0f, FLT_MAX));
	const std::string menu_label = std::string(localization::text("menu.about")) + "##about_menu";
	if (ImGui::BeginMenu(menu_label.c_str()))
	{
       ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 320.0f);
		ImGui::Text("%s", localization::text("about.title"));
		ImGui::Text("%s", localization::format("about.version", {{ "version", VERSION }}).c_str());
		ImGui::Separator();
		ImGui::TextWrapped("%s", localization::text("about.fork"));
		ImGui::BulletText("%s", localization::text("about.original_author"));
		ImGui::BulletText("%s", localization::text("about.maintainer"));
		ImGui::Spacing();
		ImGui::TextWrapped("%s", localization::text("about.support"));
		ImGui::PopTextWrapPos();

		const std::string repository_label = std::string(localization::text("about.repository")) + "##repository";
		if (ImGui::Button(repository_label.c_str()))
		{
			open_external_link(kRepositoryUrl);
		}

		ImGui::SameLine();

		const std::string issues_label = std::string(localization::text("about.issues")) + "##issues";
		if (ImGui::Button(issues_label.c_str()))
		{
			open_external_link(kIssuesUrl);
		}

		ImGui::EndMenu();
	}
}

// Renders the discovered playlist as a simple track list.
void menus::playlist()
{
	const std::string menu_label = std::string(localization::text("menu.playlist")) + "##playlist_menu";
	if (ImGui::BeginMenu(menu_label.c_str()))
	{
		audio::resolve_playlist_metadata();
		for (int i = 0; i < audio::playlist_files.size(); ++i)
		{
			if (!audio::is_track_playable(audio::playlist_files[i].first))
			{
				continue;
			}

			std::string display;
			const auto metadata = audio::playlist_metadata.find(audio::playlist_files[i].first);
			if (metadata != audio::playlist_metadata.end())
			{
				display = metadata->second.artist != "N/A"
					? metadata->second.artist + " - " + metadata->second.title
					: metadata->second.title;
			}
			else
			{
				std::string title;
				std::string artist;
				resolve_file_metadata(audio::playlist_files[i].first.c_str(), 0, title, artist);
				display = artist != "N/A" ? artist + " - " + title : title;
			}

			ImGui::Text("%s", display.c_str());
		}

		ImGui::EndMenu();
	}
}

// Loads the main UI font and merges optional emoji and Japanese glyph ranges.
void menus::build_font(ImGuiIO& io)
{
	std::string font = "ecm/fonts/NotoSans-Regular.ttf";
	std::string font_jp = "ecm/fonts/NotoSansJP-Regular.ttf";
	std::string emoji = "ecm/fonts/NotoEmoji-Regular.ttf";

	if (fs::exists(font))
	{
		// Noto Sans plus ImGui's Cyrillic range includes Latin-1 (áéíóúñ¿¡); emoji and Japanese remain merged below.
		io.Fonts->AddFontFromFileTTF(&font[0], 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

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
