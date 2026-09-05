## Context

See `proposal.md` for the motivation. The repository already has authoritative
policy, architecture, build, configuration, and CI documents, but no root
contributor entry point. The guide must remain accurate for the maintained
NFSU2 Win-x86 runtime without becoming a second application-context document.

## Goals / Non-Goals

**Goals:**

- Make the existing contribution path discoverable from one root-level file.
- Link each topic to its canonical source instead of copying implementation
  details that can drift.
- Make build, validation, approval, compatibility, and attribution boundaries
  explicit.
- Give runtime contributors a compact safety checklist covering state, audio,
  hooks, overlay/input, and persistence.

**Non-Goals:**

- Change runtime behavior, hooks, GameFlowState interpretation, audio flow,
  overlay behavior, settings, deployment, or build configuration.
- Add a new dependency, test framework, PR template, commit convention, or
  contributor automation.
- Replace `AGENTS.md` or `docs/application-context.md`.

## Decisions

1. **Use one root-level guide with links.**
   A single `CONTRIBUTING.md` is easier to discover than several new documents.
   It will link to `AGENTS.md`, `docs/application-context.md`, `README.md`,
   `BUILDING.md`, `CONFIGURATION.MD`, `.editorconfig`, issue templates, CI, and
   `openspec/config.yaml`. Duplicating the full runtime model was rejected
   because it would increase documentation drift.

2. **Separate human contribution guidance from the AI/OpenSpec note.**
   The main flow will cover issues, branches, implementation, review, and
   validation. A short dedicated section will describe the existing
   explore/plan/approval/artifacts/apply boundary without reproducing agent
   internals.

3. **Document current runtime constraints as review gates, not requirements
   changes.**
   The guide will state that runtime work must consider `GameFlowState` values
   and transitions: frontend (`LoadingFrontend`, `InFrontend`), in-game and
   loading (`UnloadingFrontend`, `LoadingRegion`, `LoadingTrack`, `Racing`),
   and the remaining states. It will point to `src/app/defs.hpp` and
   `docs/application-context.md` for the authoritative mapping. It will also
   direct reviewers to audio pause/loading/chyron behavior, package/FNG mute
   handling, `src/app/main.cpp`, `src/app/hook/hook.hpp`, audio sources,
   overlay/input sources, and settings sources.

4. **Keep validation proportional to the change.**
   The documentation-only change requires strict OpenSpec validation and
   `git diff --check`; it does not require a build. The guide will still point
   runtime contributors to the recursive-submodule setup, `generate.bat`, CI,
   and `Release | Win-x86` validation without claiming that this change has
   runtime evidence.

5. **Classify the changelog entry as `Added`.**
   Against `main`, the guide does not exist. A new contributor workflow is an
   addition, not a fix to released runtime behavior. Revisions to this
   in-progress guide remain `Changed`; no `Fixed` entry is needed.

## Risks / Trade-offs

- **Documentation drift** → Keep volatile facts in canonical documents and use
  links; add a maintenance note to update the guide when those sources or CI
  change.
- **Conflicting workflow advice** → State that `AGENTS.md` owns imperative
  policy and that the guide is a concise index, not an override.
- **False runtime expectations** → Explicitly preserve NFSU2-only, Win-x86,
  compatibility-sensitive filenames, dynamic official `bass.dll`, translation
  paths, and ECM-R/ECM/BttrDrgn attribution.
- **Incomplete validation** → State honestly that runtime validation is manual
  and that CI covers the Release Win-x86 build/output checks; require relevant
  frontend, loading, racing, overlay, pause, package, and configuration checks
  for runtime changes.

## Migration Plan

No runtime migration is required. Add `CONTRIBUTING.md`, add its Unreleased
`### Added` changelog entry, review links and commands against the canonical
files, then run the documentation-only checks. Rollback is limited to removing
the new guide and its changelog entry.

## Open Questions

None. The audience, scope, source-of-truth ownership, and workflow boundaries
were settled in the approved plan.
