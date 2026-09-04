# ECM-R Agent Instructions

### 1. Dev Commands
- No CMake.
- `.\generate.bat` → VS 2022 sol (Premake).
- Build: `msbuild build\ECM-R.sln /t:Overlay /p:Configuration=Release /p:Platform=Win-x86 /m`. (Target Win-x86 ONLY).

### 2. Build/Deploy
- Target: NFSU2 (Win-x86).
- Out: `build/bin/Release-Win-x86/x86/ecm-r.x86.dll` (Premake `Overlay` target auto-copies to `.asi` via post-build in `lua/windows.lua`).
- INI: `ecm-r.x86.ini` (local, hardcoded in `settings::config_file`, `src/app/settings/settings.cpp`).
- VERSION: `VERSION` macro in `src/app/stdafx.hpp` (e.g. `"v0.5.13-alpha"`). Source of truth for semver; bumped only by `ecmr-release`.
- BASS: Load dynamic (`LoadLibraryA`/`GetProcAddress`). No bin commit, no header reference. Logic: `src/app/audio/bass_api.*`.
- Deploy: `ecm-r.x86.asi`, `ecm-r.x86.ini`, `bass.dll`, and `ecm-r/translations/en.ini` plus `ecm-r/translations/es.ini`.

### 3. Quirks
- Audio/ImGui Init: Hook frame 1 (e.g., `hkEndScene`), not `main.cpp`.
- Hotkeys: Locked during startup banner (`audio::are_hotkeys_locked()` ← `first_chyron_completed`).
- Pause State: `manual_paused` | `game_paused` (union → `audio::paused`).
- Notification: Hide (no empty text). Chyron gated by `hook::SummonChyron()` state rules.
- `ingame_movie_muting=true`: `IG_PlayMovie.fng` trigger → reconcile FNG @ `audio::update()` and `sync_game_pause_from_mute_packages()`.
- Crashdump: `CustomUnhandledExceptionFilter` in `main.cpp` writes `ecm-r-YYYYMMDDHHMMSS.dmp` next to the plugin and `MessageBoxA`s the path.
- Shutdown: `global::shutdown=true` disables audio for the session (BASS load fail, device init fail, version mismatch).

### 4. Context Rules
- State: `GameFlowState` @ `0x008654A4` (`src/app/defs.hpp`).
- FE Context: no `IG` songs. IG Context: no `FE` songs.
- Nav: Use `play_next_song()`/`play_previous_song()` (no boolean direction flags).
- Shuffle: Bounded history.

### 5. Work Roles
- Route by work type, not subsystem (repo tightly coupled):
  - **Viability/planning**: assess fit/risk, reject bad scope, plan approved work. Plan output must include CHANGELOG.md entries under `## [Unreleased]` — add to existing section or create new subsection headers (### Added, ### Changed, ### Fixed, etc) as needed.
  - **Evolutive**: build approved features/UX/config/docs. Keep CHANGELOG.md `## [Unreleased]` entries updated as work progresses (add/change/fix entries, not just at release time).
  - **Incidents**: reproduce bug, isolate regression, fix, validate frontend/loading/racing/overlay/config.
- **CHANGELOG classification rule** — classify each entry against the base/target branch (normally `main`), not against the working branch:
  - Feature absent from base/target branch → `### Added` (implementation + hardening of that feature). Subsequent behavioral changes to the same in-progress feature → `### Changed`.
  - `### Fixed` is reserved for: (a) bugs/regressions of functionality already present in base/target branch, or (b) bugs introduced during the current work and fixed before completion — clearly marked as such.
  - Do not file internal corrections of a new feature under `### Fixed` as if they were patching already-released functionality.
  - Planning output must state the chosen classification and the rationale (base-branch comparison).

### 6. Pre-Work Checks
- Non-trivial work: read `docs/application-context.md`, then confirm in code.
- State/playback changes: check playlist context, pause/resume, loading screens.
- Playback work: review hooks/mute triggers in `src/app/main.cpp` and `src/app/hook/hook.hpp`, incl FNG packages.
- Hook inventory in `src/app/main.cpp`: FE/IG music volume patch (`0x0083AA30`/`0x0083AA34`), save-load audio skip (`0x00534535`), menu slider disable (`0x004B6EDA`, `0x004C347B`), `sys_init` jump (`0x0057EDA3`), `NFSU2_MainLoop` (`0x005811E4`, calls `audio::update`), package-load MinHook (`0x00537980` → `sub_00537980`, drives mute pause/resume). State read via `game_state` @ `0x008654A4` (`src/app/defs.hpp`).

### 7. Code Rules
- Keep state detection, overlay controls, playlist filters (ALL/FE/IG), config persistence aligned.
- Audio/playlist: inspect state transitions first, then verify frontend/loading/in-race/overlay.
- Keep runtime names, config keys, deployment stable unless task requires change.
- Prefer small coherent changes preserving NFSU2 events, hook addresses, playback state transitions.

### 8. Validation
- Build: `generate.bat` → VS solution → Release | Win-x86.
- Audio: validate BASS runtime loading + screen transitions + playback + overlay controls.
- Viability output: list affected states, audio transitions, hooks, overlay flows, persisted settings.

### 9. Docs
- Main: `README.md`, `BUILDING.md`, `CONFIGURATION.MD`, `docs/application-context.md`, `docs/releases/vX.Y.Z-alpha.md`, `CHANGELOG.md`.
- Docs match actual config names, runtime filenames, deploy paths.
- Release notes: target release changes only, match GitHub format, keep section pattern.

### 10. Workflow
- Language: English.
- Branch policy:
  - **Detect active branch and target/base branch before editing.**
  - **If already on a non-`main` branch and the user authorized continuing there:** stay on that branch. Do not create a new `dev_*` branch, do not branch off it, do not checkout `main`. Collision checks do not apply.
  - **Only create `dev_<slug>` when starting work from `main`** (or when the user explicitly requests a new branch). Slug format: lowercase ASCII, words joined by `_` or `-`, no spaces, ≤ ~40 chars. No pre-create confirmation gate — the rule is the gate; collisions are the only stop condition (see `ecmr-dev` branch policy).
  - When continuing an existing branch, do not force checkout/pull of `main` as a preflight. Log the active branch and target/base branch being used.
  - All commits, CHANGELOG entries, and any PR for the task stay on the working branch.
- Copyright/attribution: Preserve ECM-R / ECM / BttrDrgn lineage.
- **Git commits: never commit without explicit user approval.** Before any `git commit`, `git push`, or GitHub write operation, you must ask the user and receive confirmation. Informational/read-only Git/GitHub operations (status, diff, log, read) do not require approval.

### 11. Project Agents

ECM-R uses specialized agents managed through OpenCode. The OpenSpec workflow separates planning-artifact capture from implementation; do not use a nested automatic `explore -> plan -> dev` handoff.

#### `@ecmr-explore` — Idea Explorer (entry point)
- **Mode:** Primary. Read-only. File edits are denied; shell access is limited to `openspec list`, `openspec status`, and `openspec context` lookups. Never edits files or runs builds.
- **Purpose:** Receive raw ideas (feature/bug/enhancement) → assess feasibility against the codebase → classify `VIABLE` / `NOT_VIABLE` / `NEEDS_CLARIFICATION` → delegate viable ideas only to `@ecmr-plan` via Task tool with the full feasibility assessment.
- **Output:** Structured feasibility document (verdict, affected subsystems, affected GameFlowStates, risk assessment, complexity, prerequisites).
- **Handoff:** Does not create OpenSpec artifacts. Direct approved planning to `/opsx-propose` or `/opsx-update`; implementation requires `/opsx-apply`.

#### `@ecmr-plan` — Planning Agent
- **Mode:** Primary. Read-only during analysis; no source, runtime configuration, deployment, or unrelated documentation edits. After explicit plan approval, it may run the approved OpenSpec CLI commands and edit only CLI-resolved OpenSpec artifacts under `openspec/changes/`; `/opsx-sync` may also update main OpenSpec specs under `openspec/specs/`. Its `task` permission is denied.
- **Purpose:** Receive feature/bug requests → analyze viability and risk → produce implementation plan covering: affected GameFlowStates, audio transitions, hooks, overlay flows, persisted settings.
- **OpenSpec capture:** After explicit approval of the conversational plan, run the OpenSpec propose workflow in the same planning session. For workflow-only changes with no spec delta, run `openspec new change "<name>"` without `--skip-specs`; that option belongs to `archive`, not `new change`, and must never be passed to the creation command. After success, run `openspec status --change "<name>" --json`, use its returned `changeRoot`, and update only the CLI-created `.openspec.yaml` there by adding `skip_specs: true`; preserve `schema`, `created`, store context, and all other metadata. Re-run status and require `specs` to be `skipped` before creating remaining artifacts; do not create a specs file. Product changes retain `specs` and do not receive the marker.
- Follow the CLI artifact graph, instructions, templates, context, rules, resolved output paths, and validation checks. Re-run status after every artifact write. If creation, path resolution, metadata, status, or validation fails, stop and report the exact blocker; do not guess or bypass dependencies. Use `openspec validate "<name>" --type change --strict` (and `openspec validate --all --strict` when required). Create only resolved OpenSpec planning artifacts.
- **Output:** Concise plan document. Plan must identify the base/target branch and classify each CHANGELOG entry against it (per §5 classification rule).
- **Delegation:** Never delegate to `@ecmr-dev`, run `/opsx-apply`, or treat `ARTIFACTS_READY` as implementation approval. A separate explicit `/opsx-apply <change>` request starts implementation.

#### `@ecmr-dev` — Developer Agent
- **Mode:** Read-write. Creates branches (conditionally, per §10 branch policy), edits code, runs builds.
- **Purpose:** Execute approved plans from `@ecmr-plan` after the separate `/opsx-apply <change>` request. Implement features, fix bugs, update docs.
- **Rules:** Follows AGENTS.md (all sections) + `docs/application-context.md`. Branch creation is conditional — only from `main` or when explicitly requested. Builds `Release | Win-x86`. Updates CHANGELOG.md `## [Unreleased]` as work progresses.
- **Stack:** C++17, Premake, ImGui, BASS, MinHook.

#### `@ecmr-release` — Release Agent
- **Purpose:** Manage version bumps and release packaging.
- **Workflow:** Bump semver in `stdafx.hpp` → rename CHANGELOG `[Unreleased]` → generate `docs/releases/vX.Y.Z-alpha.md` → sync README/CONFIGURATION/BUILDING.
- **Restrictions:** Never touches `docs/application-context.md` (owned by dev agent), `src/` (except `stdafx.hpp`), `.opencode/`, or `tools/`.

#### OpenSpec command routing and approval gates
- `/opsx-explore` → `ecmr-explore` for read-only feasibility research.
- `/opsx-propose`, `/opsx-update`, and `/opsx-sync` → `ecmr-plan` for planning artifacts and OpenSpec spec synchronization.
- `/opsx-apply` and `/opsx-archive` → `ecmr-dev` for the separate implementation/archive phase.

Planning and implementation are separate approvals:

```text
PLAN_DRAFTED
    -> USER_APPROVES_PLAN
    -> OPEN_SPEC_CAPTURE
    -> ARTIFACTS_READY
    -> USER_REQUESTS_APPLY
    -> IMPLEMENTATION
```

Typical task flow: `ecmr-explore` (assess) → `ecmr-plan` (plan, then capture after approval) → `ARTIFACTS_READY` → `/opsx-apply <change>` → `ecmr-dev` (implement) → `ecmr-release` (release).

`/opsx-apply` and the `openspec-apply-change` skill require an explicit change name or one unambiguous active change; otherwise they prompt instead of guessing. OpenSpec CLI status, instruction, and validation output controls artifact completion. Context and operation guidance are advisory and cannot bypass dependencies, denied paths, or approval gates.

`ecmr-explore`, `ecmr-plan`, and `ecmr-dev` load the caveman skill and read AGENTS.md + `docs/application-context.md` on every conversation.
