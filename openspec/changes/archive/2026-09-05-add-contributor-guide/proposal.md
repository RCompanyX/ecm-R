## Why

Contributor instructions are currently distributed across `AGENTS.md`,
`docs/application-context.md`, `BUILDING.md`, `README.md`, CI, issue templates,
and OpenSpec guidance. A root-level contributor guide will give programmers one
entry point without duplicating the volatile runtime documentation.

## What Changes

- Add a root-level `CONTRIBUTING.md` for human contributors.
- Document the canonical source-of-truth files and the issue, branch, review,
  and approval workflow.
- Document local setup, recursive submodules, the maintained `Release | Win-x86`
  build, CI validation, and the official dynamic `bass.dll` boundary.
- Document runtime-safety expectations for NFSU2 state transitions, audio,
  hooks, overlay/input, persistence, deployment names, and attribution.
- Add a concise AI/OpenSpec workflow reference without duplicating agent rules.
- Add a `CHANGELOG.md` entry under `## [Unreleased]` / `### Added`.
- Do not change runtime source, configuration behavior, deployment files, or
  product specifications.

## Capabilities

### New Capabilities

None. This is a documentation and workflow-only change; the change metadata
sets `skip_specs: true` because no observable product behavior changes.

### Modified Capabilities

None.

## Impact

- **Documentation:** Add `CONTRIBUTING.md` and update the Unreleased changelog.
- **Runtime:** No affected `GameFlowState` behavior, audio transition, hook,
  overlay, hotkey, or persisted-setting behavior. The guide will link to the
  canonical runtime sources and describe those areas only as contributor QA
  context.
- **Build/deployment:** No build or packaging changes; preserve NFSU2-only,
  Win-x86 scope, compatibility-sensitive runtime names, translation paths, and
  the externally obtained BASS runtime.
- **Risk:** Documentation drift or conflicting instructions. Mitigate by
  assigning policy ownership to `AGENTS.md`, architecture ownership to
  `docs/application-context.md`, and linking to canonical build/configuration
  and CI documents instead of copying them.
- **Base branch:** `main`. Because `CONTRIBUTING.md` is absent there, the
  changelog entry is `### Added`, not `### Fixed`; it introduces contributor
  guidance rather than repairing released runtime behavior.
