## Why

OpenSpec 1.12.0 does not support `--skip-specs` on `openspec new change`; that
option belongs to `archive`. The planner inferred an invalid creation command,
so workflow-only proposals cannot be scaffolded and captured reliably.

## What Changes

- Document that `openspec new change "<name>"` must run without `--skip-specs`.
- Define the supported workflow for spec-driven workflow-only changes:
  scaffold first, resolve the CLI-reported `changeRoot`, add
  `skip_specs: true` to the CLI-created `.openspec.yaml` while preserving its
  metadata, then re-run status before creating artifacts.
- Require status to report the `specs` artifact as `skipped`; follow CLI
  instructions and dependency order for all remaining required artifacts.
- Clarify that `--skip-specs` is an archive-time option, not a creation-time
  option, and preserve the explicit planning-only/apply approval boundary.
- Add preflight and failure-handling guidance for duplicate change names,
  unresolved paths, and CLI status failures.

## Capabilities

### New Capabilities

None. This is a workflow and documentation correction with no spec-level
product behavior.

### Modified Capabilities

None. No existing ECM-R product requirements change. This change declares
`skip_specs: true` because it intentionally has no spec delta.

## Impact

- Tracked planner and proposal-workflow guidance in `.opencode/`, with aligned
  repository workflow context where required.
- OpenSpec 1.12.0 proposal capture and its resolved change metadata only.
- No GameFlowState, audio, hook, FNG, overlay, input, settings, runtime
  configuration, build, deployment, BASS, or user-facing ECM-R behavior changes.
- Validation is workflow-focused: CLI status/instructions/validation and
  `git diff --check`; no build or in-game validation is applicable.

## CHANGELOG Classification

Planned entry under `## [Unreleased]` → `### Changed`:

- Clarify the OpenSpec planner workflow for workflow-only changes by scaffolding
  without `--skip-specs`, then setting `skip_specs: true` in resolved change
  metadata before status and artifact capture.

Rationale: compared with target branch `main`, this corrects behavior in the
in-progress OpenSpec workflow integration already documented there. It is not
a regression in released ECM-R runtime functionality, so it is not `### Fixed`.
