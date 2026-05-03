# Copilot Instructions

## General Guidelines
- Project documentation and commit messages should be written in English.
- Project documentation and written project content should be in English.
- Repository instruction files should be written in English.
- User-facing documentation, release notes, and changelog entries should be written in English.
- Treat the Copilot agent for this repository as a specialist for analysis, code generation, and validation of ECM-R changes, with special attention to game-state transitions, audio integration, hook safety, configuration persistence, and regression validation.
- Keep fork attribution to the original ECM project and its original author, BttrDrgn.
- Do not remove or rewrite the original license attribution.
- Preserve compatibility-sensitive runtime filenames unless a task explicitly requires changing them.
- New work branches must start with the prefix `dev_`.

## Agent Focus
- Assume this fork targets **Need for Speed: Underground 2 (NFSU2)** first, with the current runtime centered on the 32-bit game integration.
- When evaluating changes, always use the game flow states defined in `src/app/defs.hpp` as the source of truth and assess how each affected state changes playlist context, pause/resume behavior, and loading-screen handling.
- When touching playback flow, also review the NFSU2-specific hooks and mute triggers in `src/app/main.cpp` and `src/app/hook/hook.hpp`, including the frontend and loading-screen FNG packages used to pause or resume music.
- Preserve coherence between game state detection, overlay controls, playlist context filtering (`ALL`, `FE`, `IG`), and configuration persistence.

## Task Modes

### Agent Routing Model
- Use a three-role operating model for repository work:
  - **Viability + planning agent**: assess new ideas, check architectural fit, reject risky or misclassified requests, and produce the implementation plan for approved evolutive work.
  - **Evolutive agent**: implement approved product improvements, UX changes, configuration changes, and directly related documentation updates.
  - **Incidents agent**: reproduce reported problems, isolate regressions, apply focused fixes, and validate that frontend, loading-screen, racing, overlay, and configuration flows still behave correctly.
- Prefer routing by **type of work** instead of by subsystem. In this repository, gameplay state, audio, hooks, overlay, and persistence are tightly coupled, so splitting by module increases handoff cost without improving ownership.
- Treat early-stage ideas as viability work first. If the request is really a bug report or a ready-to-build feature, reroute it before implementation starts.

### Code Tasks
- Keep gameplay-facing changes aligned with NFSU2 state detection, playback flow, overlay controls, and persisted configuration behavior.
- For audio or playlist work, inspect the affected game-state transitions before changing logic, then verify that frontend, loading, and in-race behavior remain coherent.
- Keep compatibility-sensitive runtime names, configuration keys, and deployment expectations stable unless the task explicitly changes them.

### Documentation Tasks
- Treat `README.md`, `BUILDING.md`, `CHANGELOG.md`, and `docs/releases/*.md` as the main maintained documentation set for this fork.
- When documenting code changes, keep configuration names, runtime filenames, deployment paths, and BASS runtime requirements exactly aligned with the current implementation.
- For release notes and changelog entries, document only the changes introduced by the target release and avoid mixing roadmap items, older features, or unmerged work.
- Prefer updating the smallest relevant documentation surface instead of rewriting multiple documents unnecessarily, but keep cross-file consistency when one change affects setup, runtime behavior, or release messaging.
- When publishing a new release, review `.github/ISSUE_TEMPLATE/` and update the templates if supported versions, reporting fields, or reporting guidance have changed.

## Code Style
- Prefer a shared helper for playlist navigation with small wrappers like `play_next_song()` and `play_previous_song()`, instead of using boolean flags for direction.

## Validation Expectations
- Validate changes against the existing Windows build flow described in `BUILDING.md`: run `generate.bat`, use the generated Visual Studio solution, and target `Release | Win-x86` when a build is required.
- For audio changes, verify both BASS runtime loading behavior and gameplay-facing behavior such as loading screens, frontend transitions, racing playback, and overlay-driven controls.
- Prefer small, coherent changes that keep NFSU2 event handling, hook addresses, and playback state transitions aligned.
- For viability work, explicitly call out which game states, audio transitions, hooks, overlay flows, and persisted settings would be affected before handing the work to the evolutive agent.

## Release Documentation
- For release documentation, only include features introduced in the target release and do not mix in items from previous releases.
- Keep release documentation focused on user-visible behavior, configuration impact, compatibility notes, and validation relevant to the tagged version.
- Preserve the ECM-R fork attribution and do not drop references that keep the original ECM/BttrDrgn lineage clear.

## Project-Specific Rules
- Migrate the project to manual dynamic loading of `bass.dll` using `LoadLibrary` and `GetProcAddress`.
- Eliminate compile-time references to BASS (`bass.h`, `bass.lib`, `deps/bass`).
- Maintain BASS only as an official runtime dependency downloaded externally.
- Keep in mind that BASS is an official runtime dependency only; do not introduce repository-managed BASS SDK binaries or unofficial redistribution flows.
- The expected deployment of the mod places `ecm-r` and `bass.dll` together within the `scripts` folder (e.g., `scripts/ecm-r.dll`, `scripts/ecm-r.ini`, `scripts/bass.dll`).
