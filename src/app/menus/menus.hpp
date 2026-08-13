#pragma once

/// Builds and renders the ImGui overlay shown on top of the game.
class menus
{
public:
	/// Builds the current ImGui widgets for the overlay frame.
	static void update();

	/// Initializes menu state and loads the overlay font resources.
	static void init();
	/// Starts a new ImGui frame for the active renderer backend.
	static void prepare();
	/// Submits the finished overlay frame to the current renderer backend.
	static void present();

	static std::string currently_playing;

private:
	/// Loads and configures the font atlas used by the overlay.
	static void build_font(ImGuiIO& io);

	/// Renders the top-level ECM-R menu bar and window toggles.
	static void main_menu_bar();
	/// Renders playback controls and runtime toggles.
	static void actions();
	/// Renders the hotkey rebinding panel and capture feedback.
	static void hotkeys();
	/// Renders version, credits, and release notice information.
	static void about();
	/// Renders the current playlist view and track selection controls.
	static void playlist();
};
