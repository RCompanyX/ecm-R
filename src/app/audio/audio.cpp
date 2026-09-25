#include "logger/logger.hpp"
#include "global.hpp"

#include "audio.hpp"
#include "bass_api.hpp"
#include "player.hpp"
#include "fs/fs.hpp"
#include "hook/hook.hpp"
#include "settings/settings.hpp"
#include "localization/localization.hpp"
#include "defs.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <numeric>
#include <random>
#include <unordered_set>

namespace
{
    constexpr int max_playback_history_entries = 50;
    std::unordered_set<std::string> unplayable_files;
	enum class initialization_state : std::uint8_t
	{
		not_started,
		initializing,
		ready,
		failed,
	};
	std::atomic<initialization_state> audio_initialization = initialization_state::not_started;
	bool has_logged_state = false;
	bool has_logged_context = false;
	std::int32_t last_logged_context = -1;
	bool last_loading_stop_active = false;

	const char* game_state_name(const GameFlowState state)
	{
		switch (state)
		{
		case GameFlowState::None:
			return "None";
		case GameFlowState::LoadingFrontend:
			return "LoadingFrontend";
		case GameFlowState::UnloadingFrontend:
			return "UnloadingFrontend";
		case GameFlowState::InFrontend:
			return "InFrontend";
		case GameFlowState::LoadingRegion:
			return "LoadingRegion";
		case GameFlowState::LoadingTrack:
			return "LoadingTrack";
		case GameFlowState::Racing:
			return "Racing";
		case GameFlowState::UnloadingTrack:
			return "UnloadingTrack";
		case GameFlowState::UnloadingRegion:
			return "UnloadingRegion";
		case GameFlowState::ExitDemoDisc:
			return "ExitDemoDisc";
		}

		return "Unknown";
	}

    struct stream_guard
    {
        bass_api::stream_handle_t handle;

        ~stream_guard()
        {
            if (handle != 0)
            {
                bass_api::stream_free(handle);
            }
        }
    };

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

	const char* playlist_context_name(const playlist_context_t context)
	{
		switch (context)
		{
		case playlist_context_t::frontend:
			return "Frontend";
		case playlist_context_t::ingame:
			return "In-game";
		default:
			return "All";
		}
	}

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

    void probe_unplayable_files()
    {
        for (const auto& track : audio::playlist_files)
        {
            if (unplayable_files.find(track.first) != unplayable_files.end())
            {
                continue;
            }

            const bass_api::stream_handle_t stream = bass_api::stream_create_file(track.first.c_str());
            if (stream != 0)
            {
                bass_api::stream_free(stream);
                continue;
            }

            if (unplayable_files.emplace(track.first).second)
            {
                logger::log_error(logger::va("BASS probe failed for '%s' (error %d); omitting from runtime playlist",
                                             track.first.c_str(), bass_api::last_call_error()));
            }
        }
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

		// ponytail: clamp title/artist to 64 code points before passing to the game chyron.
		// NFSU2's chyron has limited visual space; long embedded tags from user files
		// can overflow or cause rendering glitches. Truncation goes through wstring
		// to avoid splitting multi-byte UTF-8 sequences mid-character.
		// Upgrade path: configurable max.
		constexpr std::size_t max_chyron = 64;
		auto truncate_for_chyron = [max_chyron](const std::string& src) -> std::string
		{
			std::string s = src;
			logger::trim(s);
			if (s.size() <= max_chyron)
				return s;
			std::wstring w = fs::utf8_to_wstring(s);
			if (w.size() > max_chyron)
				w.resize(max_chyron);
			return fs::wstring_to_utf8(w);
		};
		std::string safe_title  = truncate_for_chyron(audio::currently_playing.title);
		std::string safe_artist = truncate_for_chyron(audio::currently_playing.artist);

		if (!hook::SummonChyron(fs::utf8_to_ansi(safe_title).c_str(), fs::utf8_to_ansi(safe_artist).c_str(), audio::currently_playing.where.c_str()))
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
		if (!audio::is_ready())
		{
			return;
		}

		const bool was_paused = audio::paused;

		audio::paused = audio::manual_paused || audio::game_paused;
		if (was_paused != audio::paused)
		{
			logger::log_info(logger::va("Playback %s (manual=%s, game=%s)",
				audio::paused ? "paused" : "resumed",
				audio::manual_paused ? "true" : "false",
				audio::game_paused ? "true" : "false"));
		}

		if (audio::paused)
		{
			bass_api::pause();
			if (audio::chan[0] != 0)
			{
				hook::HideChyron();
			}
		}
		else
		{
			bass_api::start();

			if (audio::chan[0] != 0 && was_paused && !audio::currently_playing.title.empty() && audio::currently_playing.title != "N/A")
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
	bool play_song_from_playlist_entry(const int playlist_entry_index, const bool record_history)
	{
		if (playlist_entry_index < 0 || playlist_entry_index >= static_cast<int>(audio::playlist_files.size()))
		{
			return false;
		}

		bool played = false;
		switch (global::state)
		{
		case GameFlowState::LoadingFrontend:
		case GameFlowState::InFrontend:
		case GameFlowState::LoadingTrack:
		case GameFlowState::LoadingRegion:
		case GameFlowState::Racing:
		default:
			played = audio::play_file(audio::playlist_files[playlist_entry_index].first, 0);
			sync_pause_state();
			break;
		}

		if (played && record_history && audio::shuffle_enabled)
		{
			record_playback_history(playlist_entry_index);
		}

		return played;
	}

	// Plays a song by its position inside the active playlist order.
    bool play_song_from_playlist_order(const int song_index, const bool record_history = true)
	{
		if (song_index < 0 || song_index >= static_cast<int>(audio::playlist_order.size()))
		{
			return false;
		}

		int playlist_entry_index = audio::playlist_order[song_index];

		if (playlist_entry_index > static_cast<int>(audio::playlist_files.size()) - 1)
		{
			playlist_entry_index = static_cast<int>(audio::playlist_files.size()) - 1;
		}

		return play_song_from_playlist_entry(playlist_entry_index, record_history);
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
		if (play_song_from_playlist_entry(playlist_entry_index, false))
		{
			return true;
		}

		audio::playback_history.erase(audio::playback_history.begin() + history_index);
		audio::playback_history_index = (std::min)(history_index, static_cast<int>(audio::playback_history.size()) - 1);
		return false;
	}

	// Moves relative to the current song, using shuffle history when available.
	void play_relative_song(const int delta)
	{
		if (audio::manual_paused)
		{
			audio::manual_paused = false;
			sync_pause_state();
		}

		if (audio::paused)
		{
			return;
		}

		audio::playlist_ended = false;

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

		const int playlist_size = static_cast<int>(audio::playlist_order.size());
		int next_song_index = audio::current_song_index + delta;

		if (next_song_index >= playlist_size)
		{
			if (delta > 0 && audio::repeat_enabled)
			{
				audio::create_playlist_order();
				next_song_index = 0;
			}
			else if (delta > 0)
			{
				audio::playlist_ended = true;
				audio::current_song_index = playlist_size;
				return;
			}
			else
			{
				next_song_index = playlist_size - 1;
			}
		}
		else if (next_song_index < 0)
		{
			next_song_index = audio::repeat_enabled ? playlist_size - 1 : 0;
		}

		for (int attempts = 0; attempts < playlist_size; ++attempts)
		{
			if (next_song_index < 0 || next_song_index >= static_cast<int>(audio::playlist_order.size()))
			{
				break;
			}

			audio::current_song_index = next_song_index;
			if (play_song_from_playlist_order(audio::current_song_index))
			{
				return;
			}

			next_song_index += delta;
			if (next_song_index >= static_cast<int>(audio::playlist_order.size()) || next_song_index < 0)
			{
				if (!audio::repeat_enabled)
				{
					break;
				}

				next_song_index = delta > 0 ? 0 : static_cast<int>(audio::playlist_order.size()) - 1;
			}
		}

		audio::playlist_ended = true;
		if (delta > 0)
		{
			audio::current_song_index = static_cast<int>(audio::playlist_order.size());
		}
	}
}

void audio::init()
{
	initialization_state expected = initialization_state::not_started;
	if (!audio_initialization.compare_exchange_strong(expected, initialization_state::initializing,
		std::memory_order_acq_rel, std::memory_order_acquire))
	{
		return;
	}

	const auto fail_initialization = []
	{
		global::shutdown.store(true, std::memory_order_release);
		audio_initialization.store(initialization_state::failed, std::memory_order_release);
	};

	if (global::shutdown.load(std::memory_order_acquire))
	{
		fail_initialization();
		return;
	}

	logger::log_info("Audio initialization started");
	rebuild_mute_detection();

   if (!bass_api::load())
	{
		const std::string error_message = localization::format("bass.load_failed", {{ "detail", bass_api::last_error() }});
		logger::log_error(logger::va("Failed to load bass.dll: %s", bass_api::last_error().c_str()));
		global::msg_box(localization::text("bass.title"), error_message);
		fail_initialization();
		return;
	}

	const DWORD loaded_version = bass_api::get_version();
	logger::log_info(logger::va("BASS loaded (version 0x%08lX)", static_cast<unsigned long>(loaded_version)));
	if (HIWORD(loaded_version) != bass_api::version)
	{
		const std::string error_message = localization::format("bass.wrong_version", {
			{ "loaded", logger::va("0x%08lX", static_cast<unsigned long>(loaded_version)) },
			{ "expected", logger::va("0x%08lX", static_cast<unsigned long>(bass_api::version)) },
		});
		logger::log_error(logger::va("Incorrect BASS version 0x%08lX (expected family 0x%08lX)",
			static_cast<unsigned long>(loaded_version), static_cast<unsigned long>(bass_api::version)));
		global::msg_box(localization::text("bass.title"), error_message);
		bass_api::unload();
		fail_initialization();
		return;
	}

   if (!bass_api::init_device(global::hwnd))
	{
		const int bass_error = bass_api::last_call_error();
		const std::string error_message = localization::format("bass.device_failed", {{ "error", std::to_string(bass_error) }});
		logger::log_error(logger::va("BASS device initialization failed (error %d)", bass_error));
		global::msg_box(localization::text("bass.title"), error_message);
		bass_api::unload();
		fail_initialization();
		return;
	}
	logger::log_info("BASS output device initialized");

	probe_unplayable_files();
	audio::create_playlist_order();
	logger::log_info(logger::va("Playlist ready: %zu discovered track(s), %zu playable track(s)", audio::playlist_files.size(), audio::playlist_order.size()));
	audio::game_paused = true;
	audio::paused = true;
	audio_initialization.store(initialization_state::ready, std::memory_order_release);
	audio::update();
}

bool audio::is_ready()
{
	return audio_initialization.load(std::memory_order_acquire) == initialization_state::ready &&
		!global::shutdown.load(std::memory_order_acquire);
}

void audio::update()
{
	if (!audio::is_ready())
	{
		return;
	}

	const GameFlowState previous_state = global::state;
	global::state = game_state;
	if (!has_logged_state || previous_state != global::state)
	{
		logger::log_info(logger::va("Game flow state: %s -> %s", game_state_name(previous_state), game_state_name(global::state)));
		has_logged_state = true;
	}

	const auto current_context = get_playlist_context();
	const std::int32_t current_context_value = static_cast<std::int32_t>(current_context);
	if (!has_logged_context || last_logged_context != current_context_value)
	{
		logger::log_info(logger::va("Playlist context changed to %s", playlist_context_name(current_context)));
		has_logged_context = true;
		last_logged_context = current_context_value;
	}

	update_first_chyron_state();
	if (audio::ingame_movie_muting)
	{
		audio::sync_game_pause_from_mute_packages();
	}

	const bool loading_stop_active = audio::stop_music_on_loading_screens && is_loading_state();
	if (loading_stop_active != last_loading_stop_active)
	{
		logger::log_info(loading_stop_active ? "Loading-screen music stop active" : "Loading-screen music stop ended");
		last_loading_stop_active = loading_stop_active;
	}

	if (loading_stop_active)
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

	const bool playlist_context_changed = audio::playlist_context != current_context_value;
	if (!audio::paused && !audio::playing && (!audio::playlist_ended || playlist_context_changed))
	{
		audio::play_next_song();
	}

	if (audio::playlist_order.empty() || audio::current_song_index < 0 || audio::current_song_index > audio::playlist_order.size() - 1)
	{
		return;
	}

}

bool audio::play_file(const std::string& file, int channel)
{
	if (!audio::is_ready())
	{
		return false;
	}

	const bool played = ::play_file(file.c_str(), channel);
	if (played)
	{
		audio::playlist_ended = false;
	}
	return played;
}

void audio::stop(int channel)
{
	if (!audio::is_ready())
	{
		return;
	}

   if (audio::chan[channel] != 0 || audio::playing)
	{
		logger::log_info(logger::va("Track stopped: %s - %s", audio::currently_playing.artist.c_str(), audio::currently_playing.title.c_str()));
	}
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
	if (!audio::is_ready())
	{
		return;
	}

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
		if (audio::is_track_playable(track.first) && is_track_valid_for_context(track.second, playlist_context))
		{
			++track_count;
		}
	}

	return track_count;
}

bool audio::is_track_playable(const std::string& file)
{
	return unplayable_files.find(file) == unplayable_files.end();
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
	if (!audio::is_ready() || audio::chan[0] == 0)
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
		if (audio::is_track_playable(audio::playlist_files[i].first) &&
			is_track_valid_for_context(audio::playlist_files[i].second, playlist_context))
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
	if (!audio::is_ready())
	{
		return;
	}

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
	if (!audio::is_ready())
	{
		return;
	}

   audio::game_paused = true;
	sync_pause_state();
}

void audio::sync_game_pause_from_mute_packages()
{
	if (!audio::is_ready())
	{
		return;
	}

	const bool should_pause = is_mute_package_loaded();
	if (should_pause == audio::game_paused)
	{
		return;
	}

	logger::log_info(should_pause ? "Mute-package state changed: pause required" : "Mute-package state changed: resume allowed");

	if (should_pause)
	{
		audio::pause();
		return;
	}

	audio::play();
}

void audio::toggle_manual_pause()
{
	if (!audio::is_ready())
	{
		return;
	}

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
	unplayable_files.clear();
	audio::playlist_metadata.clear();
	std::vector<std::string> files = fs::get_all_files(audio::playlist_dir, audio::supported_files);
	for (std::string& file : files)
	{
		audio::playlist_files.emplace_back(file, "N/A");
	}
	logger::log_info(logger::va("Playlist enumerated: %zu file(s)", files.size()));
}

void audio::resolve_playlist_metadata()
{
	if (!audio::is_ready() || !bass_api::is_available())
	{
		return;
	}

	for (const auto& track : audio::playlist_files)
	{
		if (!audio::is_track_playable(track.first))
		{
			continue;
		}

		if (audio::playlist_metadata.find(track.first) != audio::playlist_metadata.end())
		{
			continue;
		}

		playing_t metadata{"N/A", "N/A", audio::playlist_name};
		{
			const bass_api::stream_handle_t stream = bass_api::stream_create_file(track.first.c_str());
			if (stream == 0)
			{
				logger::log_error(logger::va("BASS stream open failed for metadata '%s' (error %d)",
				                             track.first.c_str(), bass_api::last_call_error()));
			}
			stream_guard guard{stream};
			resolve_file_metadata(track.first.c_str(), stream, metadata.title, metadata.artist);
		}
		audio::playlist_metadata.emplace(track.first, std::move(metadata));
	}
}

void audio::play_next_song()
{
	if (!audio::is_ready())
	{
		return;
	}

	play_relative_song(1);
}

void audio::play_previous_song()
{
	if (!audio::is_ready())
	{
		return;
	}

	play_relative_song(-1);
}

void audio::skip_to_next_track()
{
	if (!audio::is_ready())
	{
		return;
	}

	if (audio::playing)
	{
		audio::play_next_song();
	}
	else if (!audio::paused)
	{
		audio::play_next_song();
	}
}

void audio::set_shuffle_enabled(const bool enabled)
{
	if (!audio::is_ready())
	{
		return;
	}

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
	if (!audio::is_ready())
	{
		return;
	}

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
	if (!audio::is_ready())
	{
		return;
	}

	if (audio::ingame_movie_muting == enabled)
	{
		return;
	}

	audio::ingame_movie_muting = enabled;
	logger::log_info(enabled ? "In-game movie muting enabled" : "In-game movie muting disabled");
	rebuild_mute_detection();
	settings::save_config_boolean("ingame_movie_muting", audio::ingame_movie_muting);
	audio::sync_game_pause_from_mute_packages();
}

void audio::request_current_chyron()
{
	if (!audio::is_ready())
	{
		return;
	}

	audio::pending_chyron = true;
	try_show_current_chyron();
}

bool audio::are_hotkeys_locked()
{
	return !audio::first_chyron_completed;
}

void audio::set_volume(std::int32_t vol_in)
{
	if (!audio::is_ready())
	{
		return;
	}

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
bool audio::ingame_movie_muting = true;
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
std::unordered_map<std::string, playing_t> audio::playlist_metadata;
std::vector<int> audio::playlist_order;
std::vector<int> audio::playback_history;
int audio::current_song_index = 0;
int audio::playback_history_index = -1;
std::int32_t audio::playlist_context = -1;
// MP1 is retained as a legacy MPEG-1 Layer I compatibility extension; new QA targets newer formats.
std::initializer_list<std::string> audio::supported_files { "wav", "mp1", "mp2", "mp3", "ogg", "aif"};
std::vector<const char*> audio::mute_detection;
