# Contributing to ECM-R

ECM-R is a fork of the original ECM (External Custom Music) project by
BttrDrgn. This guide is a concise entry point for contributors; it does not
replace the repository policy or runtime documentation.

## Scope and source of truth

The maintained runtime targets **Need for Speed: Underground 2 (NFSU2)** and
the **Win-x86** build. Do not assume broad multi-game or x64 runtime support.

When documents overlap, use these owners:

| Topic | Source of truth |
| --- | --- |
| Imperative workflow, branch policy, validation, and agent rules | [`AGENTS.md`](AGENTS.md) |
| Runtime architecture, state flow, audio, hooks, overlay, settings, and deployment context | [`docs/application-context.md`](docs/application-context.md) |
| Product behavior and user-facing scope | [`README.md`](README.md) |
| Premake, submodules, builds, CI artifacts, and deployment setup | [`BUILDING.md`](BUILDING.md) |
| INI sections, keys, migration, and persistence | [`CONFIGURATION.MD`](CONFIGURATION.MD) |
| Formatting defaults | [`.editorconfig`](.editorconfig) |
| CI build and output checks | [`build.yml`](.github/workflows/build.yml) and [`debug.yml`](.github/workflows/debug.yml) |
| OpenSpec workflow and project context | [`openspec/config.yaml`](openspec/config.yaml) and [`.opencode/`](.opencode/) |

Keep this guide as an index. Update it when one of those canonical sources or
the CI workflow changes; do not copy volatile implementation details here.

## Before opening work

1. Read [`AGENTS.md`](AGENTS.md), [`docs/application-context.md`](docs/application-context.md),
   and the canonical document for the area you will change.
2. Search existing issues and choose the appropriate form:
   - [Bug report](.github/ISSUE_TEMPLATE/bug_report.yml) for current broken or
     regressed behavior.
   - [Feature request](.github/ISSUE_TEMPLATE/feature_request.yml) for a
     defined user-facing improvement.
   - [Idea viability](.github/ISSUE_TEMPLATE/idea_viability.yml) for an early
     idea that needs feasibility and planning first.
   - [Issue form configuration](.github/ISSUE_TEMPLATE/config.yml) keeps blank
     issues disabled and documents the documentation contact link.
3. Keep the requested scope explicit. Runtime filenames, configuration keys,
   deployment paths, and ECM-R/ECM/BttrDrgn attribution are compatibility
   contracts.

## Branches, review, and approval

When starting from `main`, use the repository branch policy:

```text
git status
git fetch origin main
git checkout main
git pull --ff-only origin main
git branch --list dev_<slug>
git ls-remote --heads origin dev_<slug>
git checkout -b dev_<slug>
```

If `git status` reports a dirty tree, stop and resolve it (stash, commit, or
abort) before updating `main` or creating the branch. Run the collision checks
before creating a new branch.

Use lowercase ASCII for `<slug>`, joined with `_` or `-`. If the local or
remote branch already exists, reuse it only when authorized or choose another
slug; do not silently branch from a stale base. If continuing an authorized
non-`main` branch, stay on it and keep all implementation, changelog, and
review work there.

Review the complete diff against `main`, keep unrelated files unchanged, and
run the checks relevant to the change before requesting review. Runtime work
needs evidence for the affected game states and transitions; documentation or
workflow work must not claim runtime or in-game validation that was not run.
There is no additional commit-message or pull-request convention defined by
this guide; follow the current repository policy and maintainer review. For
AI-assisted work, do not commit, push, open a pull request, or perform another
GitHub write without explicit user approval.

## Local setup and build

Local plugin builds require Windows, Git, Visual Studio 2022 C++ desktop build
tools with the MSVC `v143` toolset, and a Windows SDK. macOS/Linux contributors
can use the CI artifacts instead of a local Windows toolchain.

Clone with submodules, or initialize them recursively in an existing clone:

```powershell
git clone --recurse-submodules https://github.com/RCompanyX/ecm-r.git
cd ecm-r
git submodule update --init --recursive
```

The repository does not use top-level CMake. Generate the Visual Studio
solution from the repository root:

```powershell
.\generate.bat
```

Build the maintained target:

```powershell
msbuild build\ECM-R.sln /t:Overlay /p:Configuration=Release /p:Platform=Win-x86 /m
```

Expected Release outputs:

```text
build/bin/Release-Win-x86/x86/ecm-r.x86.dll
build/bin/Release-Win-x86/x86/ecm-r.x86.asi
build/bin/Release-Win-x86/x86/ecm-r.x86.pdb
build/bin/Release-Win-x86/x86/ecm-r/translations/en.ini
build/bin/Release-Win-x86/x86/ecm-r/translations/es.ini
```

The PDB is for matching crash-dump analysis and is not part of the runtime
package. The post-build step creates the `.asi` copy.

### CI validation

[`build.yml`](.github/workflows/build.yml) runs on every push and pull request.
It checks out recursive submodules, runs `generate.bat`, builds `Release | Win-x86`,
verifies the DLL, ASI, PDB, and both translation files, and uploads the build
or main artifact. A manually triggered
[`debug.yml`](.github/workflows/debug.yml) builds `Debug | Win-x86` and uploads
the DLL, ASI, and PDB as `ecm-r-debug-build`.

## Runtime and compatibility gates

The normal deployment names and layout must remain stable:

```text
Game Folder/
  scripts/
    ecm-r.x86.asi
    ecm-r.x86.ini
    bass.dll
    ecm-r/
      translations/
        en.ini
        es.ini
    Music/
```

`bass.dll` is an external official BASS Win32 runtime. Obtain it from the
[BASS page](https://www.un4seen.com/); ECM-R loads it dynamically from the
plugin directory and does not commit, bundle, or redistribute the DLL or a
static BASS SDK. Follow the official BASS license terms, including any
commercial-use requirements. Keep the repository's [MIT license](LICENSE),
the original `Copyright (c) 2022 BttrDrgn` notice, and ECM-R/ECM attribution
intact.

## Runtime change safety checklist

Use [`docs/application-context.md`](docs/application-context.md) and the code
as the authority. Before review, check the following when applicable:

- **Game flow:** confirm the `GameFlowState` mapping in
  [`src/app/defs.hpp`](src/app/defs.hpp): `LoadingFrontend` and `InFrontend`
  are frontend context; `UnloadingFrontend`, `LoadingRegion`, `LoadingTrack`,
  and `Racing` are in-game context; `None`, `UnloadingTrack`,
  `UnloadingRegion`, and `ExitDemoDisc` use the all-context fallback.
- **Loading and audio:** verify `LoadingFrontend`, `LoadingRegion`, and
  `LoadingTrack` behavior for `stop_music_on_loading_screens`; preserve the
  `manual_paused | game_paused` pause model, context filtering (`ALL`, `FE`,
  `IG`), and the first live renderer callback boundary before using BASS.
- **Chyrons and packages:** verify pause/resume chyron visibility and the
  FNG package mute flow, including `IG_PlayMovie.fng` when
  `[config] ingame_movie_muting` is enabled. Review
  [`src/app/main.cpp`](src/app/main.cpp) and
  [`src/app/hook/hook.hpp`](src/app/hook/hook.hpp).
- **Audio and hooks:** inspect the affected files under
  [`src/app/audio/`](src/app/audio/), including the dynamic BASS boundary, and
  keep NFSU2 hook and main-loop behavior coherent.
- **Overlay and input:** review [`src/app/input/`](src/app/input/) and
  [`src/app/menus/`](src/app/menus/). Preserve the first-startup-chyron
  hotkey lock, duplicate-binding protection, and suspended actions during
  hotkey capture.
- **Persistence:** keep [`src/app/settings/`](src/app/settings/) aligned with
  the `[core]`, `[config]`, `[keys]`, and `[trax]` INI sections, migration rules,
  localization, and runtime filenames.
- **Validation:** for runtime changes, cover the affected frontend, loading,
  racing, pause/resume, package, overlay, and configuration paths. Use the
  maintained `Release | Win-x86` build and report manual or in-game evidence
  separately. For documentation/workflow-only changes, run the relevant
  OpenSpec validation and `git diff --check` instead of implying runtime
  validation.

## AI and OpenSpec workflow

OpenSpec separates planning from implementation:

1. Use [`/opsx-explore`](.opencode/commands/opsx-explore.md) for read-only
   feasibility assessment.
2. Use [`/opsx-propose`](.opencode/commands/opsx-propose.md),
   [`/opsx-update`](.opencode/commands/opsx-update.md), or
   [`/opsx-sync`](.opencode/commands/opsx-sync.md) for planning artifacts.
   Planning approval produces artifacts; it does not authorize implementation.
3. Use the explicit [`/opsx-apply`](.opencode/commands/opsx-apply.md)
   command with a named change for implementation.
4. Check status, complete the CLI-resolved tasks, validate strictly where
   required, review the final diff, and archive only after implementation is
   complete.

The agent roles and permissions are defined in
[`.opencode/agents/`](.opencode/agents/) and the project-specific rules are in
[`openspec/config.yaml`](openspec/config.yaml). Do not use planning guidance
as evidence that runtime behavior was built or tested.

## Documentation-only changes

For a documentation or workflow change, keep the diff limited to the approved
documents, update `CHANGELOG.md` under `## [Unreleased]`, classify a new guide
against `main` as `### Added`, and run:

```powershell
git diff --check
openspec validate "<change-name>" --type change --strict
```

Do not edit runtime source, settings behavior, deployment files, or product
specifications unless the approved change explicitly includes them.
