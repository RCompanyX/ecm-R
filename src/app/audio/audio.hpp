#pragma once

#define BASS_SYNC_HLS_SEGMENT	0x10300
#define BASS_TAG_HLS_EXTINF		0x14000

using namespace std::literals;

/// Metadata displayed in the in-game chyron for the active track.
struct playing_t
{
   std::string title, artist, where;
};

/// Owns playlist discovery, context filtering, and playback state for ECM-R.
class audio
{
public:
	/// Loads BASS, scans the playlist, and prepares playback state.
	static void init();
	/// Opens a specific file on the requested channel and updates the current track metadata.
	static void play_file(const std::string& file, int channel);
	/// Stops playback on a single BASS channel and releases its stream.
	static void stop(int channel);
	/// Applies the global volume slider and syncs the live BASS channel volumes.
	static void set_volume(std::int32_t vol_in);
	/// Rebuilds the track list from the configured playlist directory and trax routing.
	static void enumerate_playlist();
	/// Advances playback according to the current game state, mute triggers, and track completion.
	static void update();
	/// Recomputes the playable order for the active frontend or in-game context.
  static void create_playlist_order();
	/// Randomizes the current playlist order without changing the active context.
	static void shuffle();
	/// Returns the label for the currently active playlist context.
   static const char* current_playlist_context();
	/// Reports how many tracks are available in the active playlist context.
	static int current_playlist_track_count();
	/// Resolves the effective volume for the current frontend or in-game context.
   static std::int32_t current_context_volume();
	/// Pushes the context-aware volume setting to the live BASS configuration.
	static void apply_current_context_volume();
	/// Moves to the next track in the current order, honoring shuffle and repeat.
	static void play_next_song();
	/// Moves backward through playback history or playlist order.
   static void play_previous_song();
	/// Skips the current song and starts the next playable track immediately.
	static void skip_to_next_track();
	/// Toggles the user-requested pause state independently from game-driven pauses.
	static void toggle_manual_pause();
	/// Enables or disables shuffle and rebuilds ordering when needed.
	static void set_shuffle_enabled(bool enabled);
	/// Flips shuffle mode using the current playlist state.
	static void toggle_shuffle_enabled();
	/// Enables or disables repeat for end-of-playlist navigation.
	static void set_repeat_enabled(bool enabled);
	/// Flips repeat mode.
	static void toggle_repeat_enabled();
	/// Enables or disables the experimental in-game movie mute path.
	static void set_ingame_movie_muting(bool enabled);
	/// Returns whether the current track still has a resumable BASS stream.
	static bool can_resume_current_song();
	/// Applies a game or manual pause to the active track and hides the chyron.
	static void pause();
	/// Resumes playback when a paused track can continue.
	static void play();
	/// Reconciles game-driven pause with the mute-trigger packages that are currently loaded.
	static void sync_game_pause_from_mute_packages();
	/// Marks the current track metadata to be shown again when the UI allows it.
	static void request_current_chyron();
	/// Reports whether playback hotkeys should be ignored in the current game state.
	static bool are_hotkeys_locked();

	static std::string playlist_name;
	static std::string playlist_dir;
	static std::vector<std::pair<std::string, std::string>> playlist_files;
	static std::vector<int> playlist_order;
  static std::vector<int> playback_history;
	static int current_song_index;
   static int playback_history_index;
	static std::int32_t playlist_context;

	static std::vector<const char*> mute_detection;

	static std::initializer_list<std::string> supported_files;

	static bool paused;
    static bool manual_paused;
	static bool game_paused;
	static bool playing;
	static std::int32_t req;
	static std::int32_t chan[2];

	static std::int32_t volume;
  static std::int32_t frontend_volume;
	static std::int32_t ingame_volume;
	static std::int32_t applied_volume;
	static bool stop_music_on_loading_screens;
	static bool ingame_movie_muting;
	static bool shuffle_enabled;
	static bool repeat_enabled;
	static bool playlist_ended;
	static bool pending_chyron;
	static bool first_chyron_seen;
	static bool first_chyron_completed;

	static playing_t currently_playing;
};
