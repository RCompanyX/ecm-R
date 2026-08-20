#include "logger/logger.hpp"
#include "fs/fs.hpp"
#include "global.hpp"
#include "audio.hpp"
#include "player.hpp"
#include "bass_api.hpp"
#include "hook/hook.hpp"
#include "settings/settings.hpp"

#include <algorithm>
#include <cctype>

bool play_file(const char* file, int channel)
{
   if (audio::chan[channel] != 0)
   {
      bass_api::stream_free(static_cast<std::uint32_t>(audio::chan[channel]));
      audio::chan[channel] = 0;
   }
   audio::playing = false;
   audio::applied_volume = -1;

   const auto stream = bass_api::stream_create_file(file);
   audio::chan[channel] = static_cast<std::int32_t>(stream);
   if (stream == 0)
   {
      logger::log_error(logger::va("BASS stream open failed for '%s' (error %d)",
                                   file != nullptr ? file : "<null>", bass_api::last_call_error()));
      return false;
   }

	audio::apply_current_context_volume();

  if (!bass_api::channel_play(audio::chan[channel], false))
	{
      const int error = bass_api::last_call_error();
      bass_api::stream_free(stream);
      audio::chan[channel] = 0;
      audio::applied_volume = -1;
      logger::log_error(logger::va("BASS channel play failed for '%s' (error %d)",
                                   file != nullptr ? file : "<null>", error));
      return false;
	}

	audio::playing = true;
	std::string title;
	std::string artist;
	resolve_file_metadata(file, static_cast<std::uint32_t>(audio::chan[channel]), title, artist);

	audio::currently_playing.title = title;
	audio::currently_playing.artist = artist;
	audio::currently_playing.where = audio::playlist_name;
	audio::playlist_metadata[file] = audio::currently_playing;

	logger::log_info(logger::va("Track started: '%s' (%s - %s)", file, artist.c_str(), title.c_str()));
	audio::request_current_chyron();
	return true;
}

void resolve_file_metadata(const char* file, std::uint32_t stream_handle,
                           std::string& title, std::string& artist)
{
	title = "N/A";
	artist = "N/A";

	if (stream_handle != 0)
	{
		std::string path(file);
		std::string ext;
		const size_t dot = path.rfind('.');
		if (dot != std::string::npos)
		{
			ext = path.substr(dot + 1);
			std::transform(ext.begin(), ext.end(), ext.begin(),
			               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		}
		read_metadata(stream_handle, ext, title, artist);
	}

	if (title == "N/A" || artist == "N/A")
	{
		std::string temp = file;
		temp.erase(0, audio::playlist_dir.size() + 1);
		const size_t dot = temp.rfind('.');
		if (dot != std::string::npos)
			temp.erase(dot);

		const size_t dash_pos = temp.find('-');
		if (dash_pos != std::string::npos)
		{
			if (artist == "N/A")
				artist = temp.substr(0, dash_pos);
			if (title == "N/A")
				title = temp.substr(dash_pos + 1);
		}
		else if (title == "N/A")
		{
			title = temp;
		}
	}

	logger::trim(title);
	logger::trim(artist);
}
