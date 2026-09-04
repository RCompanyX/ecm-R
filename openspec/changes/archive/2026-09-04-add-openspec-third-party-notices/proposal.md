## Why

ECM-R tracks modified OpenSpec-generated command files but currently has no centralized third-party notice for their upstream provenance and MIT licensing. Add a root-level notice so repository distributions preserve OpenSpec attribution without conflating it with ECM-R's own license.

## What Changes

- Add root-level `THIRD_PARTY_NOTICES.md` documenting `@fission-ai/openspec` version `1.12.0`, its upstream URL, copyright attribution, covered modified `.opencode/commands/opsx-*.md` files, and the complete MIT license text.
- Keep ECM-R's existing `LICENSE` (`Copyright (c) 2022 BttrDrgn`) separate and unchanged.
- Add a `### Added` entry under `CHANGELOG.md` → `## [Unreleased]` for the new compliance documentation.
- Make no source, runtime, build, deployment, configuration, or OpenSpec schema behavior changes.

## Capabilities

### New Capabilities

None. This change adds documentation only.

### Modified Capabilities

None. No spec-level product behavior changes.

This change sets `skip_specs: true` because it is documentation/compliance-only.

## Impact

- Affected files: new root `THIRD_PARTY_NOTICES.md` and the existing `CHANGELOG.md` Unreleased section.
- Affected OpenSpec-derived material: modified `.opencode/commands/opsx-*.md` files; ignored/generated `.opencode/skills/` copies are outside this notice's tracked scope.
- No impact on NFSU2 `GameFlowState`, audio playback, hooks, overlay/input flows, persisted settings, runtime filenames, or deployment layout.
- Base/target branch: `main`. The notice is absent there, so its CHANGELOG entry is `### Added`, not `### Changed` or `### Fixed`.
- Key risk: attribution and license text must match the reviewed `@fission-ai/openspec` `1.12.0` package exactly and must not imply OpenSpec ownership of ECM-R-authored modifications.
