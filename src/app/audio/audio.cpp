#include "logger/logger.hpp"
#include "global.hpp"

#include "audio.hpp"
#include "bass_api.hpp"
#include "player.hpp"
#include "fs/fs.hpp"
#include "hook/hook.hpp"
#include "settings/settings.hpp"
#include "defs.hpp"

#include <algorithm>
#include <numeric>
#include <random>

namespace
{
    constexpr int max_playback_history_entries = 50;

	// Keeps persisted and context-specific volumes inside ECM-R's 0-100 range.
	std::int32_t clamp_volume(const std::int32_t volume)
	{
		return std::clamp(volume, 0, 100);
	}

	enum class playlist_context_t : std::int32_t
	{
		all,
		frontend,
		ingame
	};

	// Maps the current NFSU2 flow state to the playlist context used by ECM-R.
	playlist_context_t get_playlist_context()
	{
        global::state = game_state;

		switch (global::state)
		{
       case GameFlowState::LoadingFrontend:
		case GameFlowState::InFrontend:
			return playlist_context_t::frontend;

      case GameFlowState::LoadingTrack:
		case GameFlowState::LoadingRegion:
		case GameFlowState::Racing:
      case GameFlowState::UnloadingFrontend:
			return playlist_context_t::ingame;

		default:
			return playlist_context_t::all;
		}
	}

	// Filters a track according to its [trax] routing and the active playlist context.
	bool is_track_valid_for_context(const std::string& track_context, const playlist_context_t playlist_context)
	{
		if (track_context == "ALL" || track_context == "N/A")
		{
			return true;
		}

		switch (playlist_context)
		{
		case playlist_context_t::frontend:
			return track_context != "IG";

		case playlist_context_t::ingame:
			return track_context != "FE";

		default:
			return true;
		}
	}

	// Reports whether the game is currently inside one of the loading flows that can stop music.
	bool is_loading_state()
	{
		global::state = game_state;

		return global::state == GameFlowState::LoadingFrontend ||
			global::state == GameFlowState::LoadingRegion ||
			global::state == GameFlowState::LoadingTrack;
	}

	// Resolves the playlist entry index that matches the current song order position.
	int current_playlist_entry_index()
	{
		if (audio::playlist_order.empty() || audio::current_song_index < 0 || audio::current_song_index >= static_cast<int>(audio::playlist_order.size()))
		{
			return -1;
		}

		return audio::playlist_order[audio::current_song_index];
	}

	// Attempts to show the current track chyron once audio and UI state are both ready.
	bool try_show_current_chyron()
	{
		if (audio::paused || audio::chan[0] == 0 || audio::currently_playing.title.empty() || audio::currently_playing.title == "N/A")
		{
			return false;
		}

		if (!hook::SummonChyron(fs::utf8_to_ansi(audio::currently_playing.title).c_str(), fs::utf8_to_ansi(audio::currently_playing.artist).c_str(), audio::currently_playing.where.c_str()))
		{
			return false;
		}

		if (!hook::IsPackageLoaded("Chyron_FE.fng") && !hook::IsPackageLoaded("Chyron_IG.fng"))
		{
			return false;
		}

		audio::pending_chyron = false;
		return true;
	}

	// Tracks whether the first chyron has fully appeared and disappeared at least once.
	void update_first_chyron_state()
	{
		if (audio::first_chyron_completed)
		{
			return;
		}

		const bool chyron_loaded = hook::IsPackageLoaded("Chyron_FE.fng") || hook::IsPackageLoaded("Chyron_IG.fng");
		if (chyron_loaded)
		{
			audio::first_chyron_seen = true;
		}
		else if (audio::first_chyron_seen)
		{
			audio::first_chyron_completed = true;
		}
	}

	// Reports whether any FNG package that should mute ECM-R is currently active.
	bool is_mute_package_loaded()
	{
		for (const char* package : audio::mute_detection)
		{
			if (hook::IsPackageLoaded(package))
			{
				return true;
			}
		}

		return false;
	}

	// Rebuilds the list of FNG packages that should pause ECM-R for the current game.
	void rebuild_mute_detection()
	{
		audio::mute_detection.clear();

		switch (global::game)
		{
		case game_t::NFSU2:
			audio::mute_detection.emplace_back("LS_PSAMovie.fng");
			audio::mute_detection.emplace_back("LS_THXMovie.fng");
			audio::mute_detection.emplace_back("LS_EAlogo.fng");
			audio::mute_detection.emplace_back("LS_BlankMovie.fng");
			audio::mute_detection.emplace_back("UG_LS_IntroFMV.fng");
			if (audio::ingame_movie_muting)
			{
				audio::mute_detection.emplace_back("IG_PlayMovie.fng");
			}
			break;
		}
	}

	// Applies the combined manual and game-driven pause state to the active stream.
	void sync_pause_state()
	{
		const bool was_paused = audio::paused;

		audio::paused = audio::manual_paused || audio::game_paused;

		if (audio::chan[0] == 0)
		{
			return;
		}

		if (audio::paused)
		{
			bass_api::pause();
           hook::HideChyron();
		}
		else
		{
			bass_api::start();

			if (was_paused && !audio::currently_playing.title.empty() && audio::currently_playing.title != "N/A")
			{
				audio::request_current_chyron();
			}
		}
	}

	// Resets shuffle history so backward navigation starts from a clean slate.
	void clear_playback_history()
	{
		audio::playback_history.clear();
		audio::playback_history_index = -1;
	}

	// Records a visited playlist entry so shuffled playback can move backward and forward.
	void record_playback_history(const int playlist_entry_index)
	{
		if (audio::playback_history_index + 1 < static_cast<int>(audio::playback_history.size()))
		{
			audio::playback_history.erase(audio::playback_history.begin() + audio::playback_history_index + 1, audio::playback_history.end());
		}

		if (static_cast<int>(audio::playback_history.size()) >= max_playback_history_entries)
		{
			audio::playback_history.erase(audio::playback_history.begin());
			if (audio::playback_history_index > 0)
			{
				--audio::playback_history_index;
			}
		}

		audio::playback_history.emplace_back(playlist_entry_index);
		audio::playback_history_index = static_cast<int>(audio::playback_history.size()) - 1;
	}

	// Aligns the current song order position with a concrete playlist entry index.
	void sync_current_song_index_from_playlist_entry(const int playlist_entry_index)
	{
		const auto it = std::find(audio::playlist_order.begin(), audio::playlist_order.end(), playlist_entry_index);
		if (it != audio::playlist_order.end())
		{
			audio::current_song_index = static_cast<int>(std::distance(audio::playlist_order.begin(), it));
		}
	}

	// Plays a concrete playlist entry and optionally records it in shuffle history.
	void play_song_from_playlist_entry(const int playlist_entry_index, const bool record_history)
	{
		if (playlist_entry_index < 0 || playlist_entry_index >= static_cast<int>(audio::playlist_files.size()))
		{
			return;
		}

     if (record_history && audio::shuffle_enabled)
		{
			record_playback_history(playlist_entry_index);
		}

		switch (global::state)
		{
		case GameFlowState::LoadingFrontend:
		case GameFlowState::InFrontend:
		case GameFlowState::LoadingTrack:
		case GameFlowState::LoadingRegion:
		case GameFlowState::Racing:
		default:
			audio::play_file(audio::playlist_files[playlist_entry_index].first, 0);
          sync_pause_state();
			break;
		}
	}

	// Plays a song by its position inside the active playlist order.
    void play_song_from_playlist_order(const int song_index, const bool record_history = true)
	{
		if (song_index < 0 || song_index >= static_cast<int>(audio::playlist_order.size()))
		{
			return;
		}

		int playlist_entry_index = audio::playlist_order[song_index];

		if (playlist_entry_index > static_cast<int>(audio::playlist_files.size()) - 1)
		{
			playlist_entry_index = static_cast<int>(audio::playlist_files.size()) - 1;
		}

		play_song_from_playlist_entry(playlist_entry_index, record_history);
	}

	// Rebuilds playlist ordering when the active frontend or in-game context changes.
	bool ensure_playlist_order_for_current_context(const int reset_index)
	{
		const auto playlist_context = static_cast<std::int32_t>(get_playlist_context());
		if (audio::playlist_order.empty() || audio::playlist_context != playlist_context)
		{
			audio::create_playlist_order();
			audio::current_song_index = reset_index;
		}

		return !audio::playlist_order.empty();
	}

	// Moves the current song pointer while honoring repeat and end-of-playlist state.
	void move_current_song_index(const int delta)
	{
		const int last_song_index = static_cast<int>(audio::playlist_order.size()) - 1;
		const int next_song_index = audio::current_song_index + delta;

		if (next_song_index > last_song_index)
		{
			if (!audio::repeat_enabled)
			{
				audio::playlist_ended = true;
				audio::current_song_index = static_cast<int>(audio::playlist_order.size());
				return;
			}

			audio::create_playlist_order();
			audio::current_song_index = 0;
			return;
		}

		if (next_song_index < 0)
		{
			audio::current_song_index = audio::repeat_enabled ? last_song_index : 0;
			return;
		}

		audio::current_song_index = next_song_index;
	}

	// Restores a previously visited shuffle entry if the requested history position exists.
	bool try_play_song_from_history(const int history_index)
	{
		if (history_index < 0 || history_index >= static_cast<int>(audio::playback_history.size()))
		{
			return false;
		}

		const int playlist_entry_index = audio::playback_history[history_index];

		// Validate track is valid for current playlist context
		const auto track_ctx = audio::playlist_files[playlist_entry_index].second;
		const auto current_ctx = get_playlist_context();
		if (!is_track_valid_for_context(track_ctx, current_ctx))
		{
			audio::playback_history.erase(audio::playback_history.begin() + history_index);
			if (audio::playback_history_index > history_index)
				--audio::playback_history_index;
			else if (audio::playback_history_index == history_index
			         && audio::playback_history_index >= static_cast<int>(audio::playback_history.size()))
				audio::playback_history_index = static_cast<int>(audio::playback_history.size()) - 1;
			return false;
		}

		sync_current_song_index_from_playlist_entry(playlist_entry_index);
		play_song_from_playlist_entry(playlist_entry_index, false);
		return true;
	}

	// Moves relative to the current song, using shuffle history when available.
	void play_relative_song(const int delta)
	{
		const int reset_index = delta > 0 ? -1 : static_cast<int>(audio::playlist_order.size());
		if (!ensure_playlist_order_for_current_context(reset_index))
		{
			return;
		}

		if (audio::chan[0] != 0)
		{
			audio::stop(0);
		}

		if (audio::shuffle_enabled)
		{
			if (delta < 0 && audio::playback_history_index > 0)
			{
				--audio::playback_history_index;
				if (try_play_song_from_history(audio::playback_history_index))
				{
					return;
				}
			}
			else if (delta < 0 && audio::playback_history_index == 0)
			{
				if (try_play_song_from_history(audio::playback_history_index))
				{
					return;
				}
			}

			if (delta > 0 && audio::playback_history_index >= 0 && audio::playback_history_index < static_cast<int>(audio::playback_history.size()) - 1)
			{
				++audio::playback_history_index;
				if (try_play_song_from_history(audio::playback_history_index))
				{
					return;
				}
			}
		}

		move_current_song_index(delta);

		if (audio::current_song_index < 0 || audio::current_song_index >= static_cast<int>(audio::playlist_order.size()))
		{
			return;
		}

		play_song_from_playlist_order(audio::current_song_index);
	}
}

void audio::init()
{
	rebuild_mute_detection();

   if (!bass_api::load())
	{
		const std::string error_message = logger::va(
			"Failed to load bass.dll.\n"
			"%s\n"
			"Make sure bass.dll is in the scripts folder next to ecm-r.x86.asi.\n"
			"See the mod README for where to get bass.dll and where to place it.\n"
			"ECM-R music will be disabled for this session.",
			bass_api::last_error().c_str());
		logger::log_error(logger::va("Failed to load bass.dll: %s", bass_api::last_error().c_str()));
		global::msg_box("ECM-R BASS", error_message.c_str());
		global::shutdown = true;
		return;
	}

	if (HIWORD(bass_api::get_version()) != bass_api::version)
	{
      global::msg_box("ECM-R BASS", "An incorrect version of BASS.DLL was loaded!");
		global::shutdown = true;
       bass_api::unload();
		return;
	}

  if (!bass_api::init_device(global::hwnd))
	{
      global::msg_box("ECM-R BASS", "Can't initialize device!\nNo audio will play for this session!");
       bass_api::unload();
		global::shutdown = true;
		return;
	}

  audio::create_playlist_order();
	audio::pause();
	audio::update();
}

void audio::update()
{
	global::state = game_state;
	update_first_chyron_state();
	if (audio::ingame_movie_muting)
	{
		audio::sync_game_pause_from_mute_packages();
	}

   if (audio::stop_music_on_loading_screens && is_loading_state())
	{
     if (audio::chan[0] != 0 || audio::playing)
		{
			audio::stop(0);
		}

		return;
	}

	audio::apply_current_context_volume();

 std::uint32_t state = bass_api::channel_is_active(audio::chan[0]);

	switch (state)
	{
   case bass_api::active_stopped:
		audio::playing = false;
		break;
	}

	if (audio::pending_chyron)
	{
		try_show_current_chyron();
	}

	if (!audio::paused && !audio::playing)
	{
		audio::play_next_song();
	}

	if (audio::playlist_order.empty() || audio::current_song_index < 0 || audio::current_song_index > audio::playlist_order.size() - 1)
	{
		return;
	}

}

void audio::play_file(const std::string& file, int channel)
{
	audio::playlist_ended = false;
	::play_file(file.c_str(), channel);
}

void audio::stop(int channel)
{
  audio::paused = audio::manual_paused || audio::game_paused;
	audio::playing = false;

  bass_api::stream_free(audio::chan[channel]);
	audio::chan[channel] = 0;
	audio::applied_volume = -1;

	audio::currently_playing.title = "N/A";
   audio::currently_playing.artist = "N/A";
	audio::currently_playing.where = "N/A";
	audio::pending_chyron = false;
}

void audio::shuffle()
{
	audio::create_playlist_order();
}

const char* audio::current_playlist_context()
{
	switch (get_playlist_context())
	{
	case playlist_context_t::frontend:
		return "Frontend";

	case playlist_context_t::ingame:
		return "In-game";

	default:
		return "All";
	}
}

int audio::current_playlist_track_count()
{
	const auto playlist_context = get_playlist_context();
	int track_count = 0;

	for (const auto& track : audio::playlist_files)
	{
		if (is_track_valid_for_context(track.second, playlist_context))
		{
			++track_count;
		}
	}

	return track_count;
}

std::int32_t audio::current_context_volume()
{
	switch (get_playlist_context())
	{
	case playlist_context_t::frontend:
		return clamp_volume(audio::frontend_volume);

	case playlist_context_t::ingame:
		return clamp_volume(audio::ingame_volume);

	default:
		return clamp_volume(audio::volume);
	}
}

void audio::apply_current_context_volume()
{
	if (audio::chan[0] == 0)
	{
		return;
	}

	const std::int32_t volume = audio::current_context_volume();
	if (audio::applied_volume == volume)
	{
		return;
	}

	if (bass_api::set_channel_volume(audio::chan[0], static_cast<float>(volume) / 100.0f))
	{
		audio::applied_volume = volume;
	}
}

void audio::create_playlist_order()
{
	audio::playlist_order.clear();
	audio::playlist_ended = false;

	if (!audio::shuffle_enabled)
	{
		clear_playback_history();
	}

	if (audio::playlist_files.empty())
	{
		return;
	}

	const auto playlist_context = get_playlist_context();
	audio::playlist_context = static_cast<std::int32_t>(playlist_context);

	for (int i = 0; i < audio::playlist_files.size(); ++i)
	{
		if (is_track_valid_for_context(audio::playlist_files[i].second, playlist_context))
		{
			audio::playlist_order.emplace_back(i);
		}
	}

	if (audio::playlist_order.empty())
	{
		return;
	}

	if (audio::shuffle_enabled)
	{
        static std::random_device rd;
		static std::mt19937 mt(rd());
		std::shuffle(audio::playlist_order.begin(), audio::playlist_order.end(), mt);
	}
}

void audio::play()
{
	const bool can_resume_current_song = audio::can_resume_current_song();

  audio::game_paused = false;

	if (!can_resume_current_song)
	{
		audio::stop(0);
	}

	sync_pause_state();

	if (!audio::paused && !audio::playing && !is_loading_state())
	{
		audio::play_next_song();
	}
}

void audio::pause()
{
   audio::game_paused = true;
	sync_pause_state();
}

void audio::sync_game_pause_from_mute_packages()
{
	const bool should_pause = is_mute_package_loaded();
	if (should_pause == audio::game_paused)
	{
		return;
	}

	if (should_pause)
	{
		audio::pause();
		return;
	}

	audio::play();
}

void audio::toggle_manual_pause()
{
	const bool can_resume_current_song = audio::can_resume_current_song();

	audio::manual_paused = !audio::manual_paused;

	if (!audio::manual_paused && !can_resume_current_song)
	{
		audio::stop(0);
	}

	sync_pause_state();

	if (!audio::paused && !audio::playing && !is_loading_state())
	{
		audio::play_next_song();
	}
}

bool audio::can_resume_current_song()
{
	if (audio::chan[0] == 0)
	{
		return false;
	}

	const int playlist_entry_index = current_playlist_entry_index();
	if (playlist_entry_index < 0 || playlist_entry_index >= static_cast<int>(audio::playlist_files.size()))
	{
		return false;
	}

	return is_track_valid_for_context(audio::playlist_files[playlist_entry_index].second, get_playlist_context());
}

void audio::enumerate_playlist()
{
	std::vector<std::string> files = fs::get_all_files(audio::playlist_dir, audio::supported_files);
	for (std::string& file : files)
	{
		audio::playlist_files.emplace_back(file, "N/A");
	}
}

void audio::play_next_song()
{
    play_relative_song(1);
}

void audio::play_previous_song()
{
    play_relative_song(-1);
}

void audio::skip_to_next_track()
{
	if (audio::playing)
	{
		audio::stop(0);
		audio::play_next_song();
	}
	else if (!audio::paused)
	{
		audio::play_next_song();
	}
}

void audio::set_shuffle_enabled(const bool enabled)
{
	if (audio::shuffle_enabled == enabled)
	{
		return;
	}

	audio::shuffle_enabled = enabled;
	audio::create_playlist_order();
	audio::current_song_index = -1;
	settings::save_config_boolean("shuffle_enabled", audio::shuffle_enabled);
}

void audio::toggle_shuffle_enabled()
{
	audio::set_shuffle_enabled(!audio::shuffle_enabled);
}

void audio::set_repeat_enabled(const bool enabled)
{
	if (audio::repeat_enabled == enabled)
	{
		return;
	}

	audio::repeat_enabled = enabled;
	settings::save_config_boolean("repeat_enabled", audio::repeat_enabled);
}

void audio::toggle_repeat_enabled()
{
	audio::set_repeat_enabled(!audio::repeat_enabled);
}

void audio::set_ingame_movie_muting(const bool enabled)
{
	if (audio::ingame_movie_muting == enabled)
	{
		return;
	}

	audio::ingame_movie_muting = enabled;
	rebuild_mute_detection();
	settings::save_experimental_boolean("ingame_movie_muting", audio::ingame_movie_muting);
	audio::sync_game_pause_from_mute_packages();
}

void audio::request_current_chyron()
{
	audio::pending_chyron = true;
	try_show_current_chyron();
}

bool audio::are_hotkeys_locked()
{
	return !audio::first_chyron_completed;
}

void audio::set_volume(std::int32_t vol_in)
{
   const std::int32_t volume = clamp_volume(vol_in);
	if (audio::chan[0] != 0 && bass_api::set_channel_volume(audio::chan[0], static_cast<float>(volume) / 100.0f))
	{
		audio::applied_volume = volume;
	}
}

bool audio::paused = false;
bool audio::manual_paused = false;
bool audio::game_paused = false;
bool audio::playing = false;
std::int32_t audio::req;
std::int32_t audio::chan[2];
std::int32_t audio::volume = 50;
std::int32_t audio::frontend_volume = 50;
std::int32_t audio::ingame_volume = 50;
std::int32_t audio::applied_volume = -1;
bool audio::stop_music_on_loading_screens = true;
bool audio::ingame_movie_muting = false;
bool audio::shuffle_enabled = true;
bool audio::repeat_enabled = true;
bool audio::playlist_ended = false;
bool audio::pending_chyron = false;
bool audio::first_chyron_seen = false;
bool audio::first_chyron_completed = false;
playing_t audio::currently_playing = {"N/A", "N/A", "N/A"};
std::string audio::playlist_name = "Music";
std::string audio::playlist_dir = "Music";
std::vector<std::pair<std::string, std::string>> audio::playlist_files;
std::vector<int> audio::playlist_order;
std::vector<int> audio::playback_history;
int audio::current_song_index = 0;
int audio::playback_history_index = -1;
std::int32_t audio::playlist_context = -1;
std::initializer_list<std::string> audio::supported_files { "wav", "mp1", "mp2", "mp3", "ogg", "aif"};
std::vector<const char*> audio::mute_detection;
