## Plan: Stop Music During In-Game Movies

Recommended approach: treat in-game movies as a dedicated playback suppression reason, separate from the current pause-only FNG list. The current working hypothesis, supported strongly by the available reverse-engineering docs but not yet proven as absolute for every comic-style sequence, is that IG_PlayMovie.fng is the primary in-game movie screen to detect. Use NFSU2 package presence as the ground truth for the full movie lifetime under that hypothesis, stop the current ECM-R stream on entry, keep playback blocked while the movie package remains loaded, and restart normal track selection only after the movie package is gone. This matches the requested stop behavior better than pause/resume, avoids immediate auto-restart inside audio::update(), and is less brittle than relying on the next non-target ShowFNG call.

**Steps**
1. Discovery lock-in: document the current control path before implementation. The current hook is ShowFNG at 0x537980 in src/app/main.cpp, which currently classifies packages through audio::mute_detection and calls audio::pause()/audio::play() based on package name only. NFSU2 reverse-engineering docs identify IG_PlayMovie.fng as the best-documented in-game movie screen. The most explicit code path currently documented is the post-race route through PostRaceFNGObject::PlayMovieIfNeeded and Career::GetMovieNameToPlay in nfsu2-re/docs/funcs.html. That makes IG_PlayMovie.fng a strong implementation hypothesis for comic-style movies, but not a proven universal guarantee for every before-race sequence until runtime validation confirms it.
2. Add a dedicated in-game movie detector in the audio layer, not by extending the existing mute_detection list. Recommendation: introduce a helper that classifies in-game movie packages separately, seeded with IG_PlayMovie.fng as the initial detector under the current hypothesis. This keeps loading/boot movie handling and in-game movie handling from sharing the same pause semantics, keeps the detector focused on the actual in-game movie screen rather than on whether the movie happens before or after a race, and leaves one explicit place to extend if runtime verification reveals another package.
3. Add a dedicated playback suppression state for in-game movies in src/app/audio/audio.hpp and src/app/audio/audio.cpp. Recommendation: use a named flag such as ingame_movie_active or stop_for_ingame_movie instead of overloading game_paused, because the requested behavior is stop-and-restart, not pause-and-resume. The paused/blocked logic should prevent audio::update() from auto-starting a new track while the movie is active.
4. Drive the movie lifecycle from package-loaded state, not from the next unrelated ShowFNG. Reuse hook::IsPackageLoaded in src/app/hook/hook.hpp and poll for IG_PlayMovie.fng from audio::update(). On transition to loaded: stop the current channel, clear any pending chyron, mark the movie suppression active. While loaded: suppress auto-play. On transition to unloaded: clear the suppression and allow the normal next-track flow to resume once. This step depends on step 3.
5. Narrow the existing ShowFNG hook in src/app/main.cpp so it no longer tries to manage in-game movie exit implicitly. Recommendation: keep ShowFNG as an optional fast entry hint for known packages if useful, but do not rely on the current else-if audio::game_paused -> audio::play() pattern for in-game movies, because any auxiliary FNG load during the movie could resume music too early. This step depends on steps 2 and 4.
6. Decide configuration scope. Recommended default: add a dedicated boolean setting such as stop_music_on_ingame_movies = true in src/app/settings/settings.cpp, mirroring stop_music_on_loading_screens. This keeps behavior explicit, supports migration of existing INI files, and avoids silently broadening runtime behavior without user control. If scope must stay minimal, hardcode the behavior first and defer the setting, but that is the weaker product choice.
7. Update documentation surfaces if step 6 is included: CONFIGURATION.MD and README.md should describe the new movie handling behavior and its relation to loading-screen handling. Change log or release notes should only be updated if the implementation is prepared for a release branch.
8. Validation phase: build with the existing Windows flow, then validate the exact gameplay sequence. Confirm that entering an in-game movie stops ECM-R audio immediately, no replacement song starts during the full movie, leaving the movie restarts normal playlist flow once, and the same behavior holds for a comic-style movie shown before a race and for one shown after a race. This phase must also confirm or falsify the current IG_PlayMovie.fng hypothesis for the before-race case; if it is falsified, extend the centralized detector with the actual package instead of reworking the broader architecture. Loading-screen behavior must remain governed by stop_music_on_loading_screens, manual pause must still win over automatic resume, and chyron/pause-menu behavior must not regress. This step depends on steps 3 through 7.

**Relevant files**
- src/app/main.cpp — current ShowFNG hook at 0x537980; current pause/resume package logic.
- src/app/audio/audio.hpp — add dedicated movie suppression state and any new classifier declarations.
- src/app/audio/audio.cpp — current mute_detection initialization, sync_pause_state, update loop, stop/play behavior, and autoplay guard.
- src/app/hook/hook.hpp — existing IsPackageLoaded helper for package-presence polling.
- src/app/settings/settings.cpp — optional INI key, migration, and default config text.
- CONFIGURATION.MD — document the new user-facing setting if included.
- README.md — update feature/config summary if included.
- nfsu2-re/docs/funcs.html — references for PostRaceFNGObject::PlayMovieIfNeeded, Career::GetMovieNameToPlay, FNGINIT_IG_PlayMovie.fng, and IGPlayMovieFNGObject.
- nfsu2-re/docs/docs.txt — FNG registry entry for IG_PlayMovie.fng and UI element type 7 movie context.
- bass_docs/BASS_ChannelPause.html, bass_docs/BASS_ChannelStop.html, bass_docs/BASS_ChannelSlideAttribute.html, bass_docs/BASS_ATTRIB_VOL.html — reviewed to compare pause/resume, stop, and optional fade strategies.

**Verification**
1. Build ECM-R with the documented Windows flow in BUILDING.md and target Release | Win-x86.
2. In career gameplay, trigger a known comic-style in-game movie before a race and verify ECM-R stops the current custom song immediately when IG_PlayMovie.fng becomes active, or record the actual package if a different in-game movie screen is used.
3. Confirm no new ECM-R song auto-starts while the movie is still on screen, even if other UI packages are shown during the sequence.
4. Trigger a known comic-style in-game movie after a race and verify the same stop-and-hold behavior is preserved there as well, confirming that the documented post-race route still matches the detector used by the implementation.
5. Exit the movie and verify ECM-R restarts normal playback flow once, respecting the current FE/IG playlist context.
6. Re-test loading screens with stop_music_on_loading_screens both true and false to confirm no regression in existing behavior.
7. Re-test manual pause, next/previous track controls, and chyron visibility to ensure the new movie suppression state does not break current pause/resume assumptions.

**Decisions**
- Included scope: any known comic-style in-game movie flow, whether it appears before or after a race, implemented through a centralized in-game movie detector that currently starts with IG_PlayMovie.fng as the strongest documented hypothesis.
- Excluded scope: boot/loading movie package rewrites, deep reverse-engineered hooks into IGPlayMovieFNGObject::MessageHandler, and cosmetic fade-out work for the first pass.
- Recommended runtime behavior: stop the current song on movie entry and resume normal track selection after movie exit, not pause/resume the same song.
- Recommended architecture: package-loaded polling as the authoritative exit signal, because it is less brittle than depending on the next unrelated ShowFNG event. The detector itself should remain easy to extend if runtime validation shows that not every relevant sequence uses IG_PlayMovie.fng.

**Further Considerations**
1. Optional polish after the core fix: add a short fade-out on movie entry via BASS_ChannelSlideAttribute and BASS_ATTRIB_VOL. This would require extending src/app/audio/bass_api.cpp and src/app/audio/bass_api.hpp to resolve additional BASS exports, so it should be a follow-up, not part of the first correction.
2. If runtime validation or future reverse-engineering uncovers more in-game movie packages than IG_PlayMovie.fng, extend the centralized detector list instead of spreading more string checks through main.cpp.
3. If the first implementation still shows premature resume, escalate one level deeper to an IG_PlayMovie-specific hook or unload signal instead of widening unrelated FNG heuristics.

**Reference usage note**
- The external reverse-engineering material from yugecin/nfsu2-re was used as reference material for symbols, package names, and likely control flow only.
- ECM-R implementation work should remain independently authored in this repository and validated against runtime behavior in the game.
- Keep the README attribution to yugecin/nfsu2-re as a research reference.
- Do not copy or vendor code or documentation text from that repository into ECM-R without explicit permission.
