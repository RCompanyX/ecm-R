#include "logger/logger.hpp"
#include "fs/fs.hpp"
#include "global.hpp"
#include "audio.hpp"
#include "bass_api.hpp"
#include "hook/hook.hpp"
#include "settings/settings.hpp"

#include <algorithm>

void play_file(const char* file, int channel)
{
   audio::chan[channel] = static_cast<std::int32_t>(bass_api::stream_create_file(file));
	audio::apply_current_context_volume();

  if (audio::chan[channel] != 0 && bass_api::channel_play(audio::chan[channel], false))
	{
		audio::playing = true;
		std::string title = "N/A";
		std::string artist = "N/A";

		// Try embedded metadata first
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
			read_metadata(static_cast<std::uint32_t>(audio::chan[channel]), ext, title, artist);
		}

		// Per-field filename fallback — only fill fields still "N/A"
		if (title == "N/A" || artist == "N/A")
		{
			std::string temp = file;
			// Strip playlist directory prefix
			temp.erase(0, audio::playlist_dir.size() + 1);
			// Strip file extension (find last dot)
			const size_t dot = temp.rfind('.');
			if (dot != std::string::npos)
				temp.erase(dot);

			const size_t dash_pos = temp.find('-');
			if (dash_pos != std::string::npos)
			{
				if (artist == "N/A")
					artist = temp.substr(0, dash_pos);
				if (title == "N/A")
					title  = temp.substr(dash_pos + 1); // skip '-'
			}
			else if (title == "N/A")
			{
				title = temp;
			}
		}

		logger::trim(title);
		logger::trim(artist);

		audio::currently_playing.title = title;
		audio::currently_playing.artist = artist;
		audio::currently_playing.where = audio::playlist_name;

		audio::request_current_chyron();
	}
}
