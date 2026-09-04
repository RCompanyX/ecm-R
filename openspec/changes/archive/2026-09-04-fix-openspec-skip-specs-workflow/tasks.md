## 1. Correct planner workflow guidance

- [x] 1.1 Update `.opencode/agents/ecmr-plan.md` to distinguish `new change` options from `archive` options, require `openspec new change "<name>"` without `--skip-specs`, and document the post-scaffold metadata step; verify the prompt contains no unsupported creation flag.
- [x] 1.2 Align `.opencode/commands/opsx-propose.md`, `AGENTS.md`, and `docs/application-context.md` with the same `changeRoot`/`.openspec.yaml`/status sequence while preserving store handling and approval gates; verify tracked guidance gives one consistent workflow and does not modify generated or installed OpenSpec files.

## 2. Validate workflow behavior

- [x] 2.1 Verify a spec-driven workflow-only change is scaffolded with `openspec new change "<name>"` and no `--skip-specs`, then add only `skip_specs: true` to the CLI-created metadata while preserving `schema`, `created`, and other fields; verify the command completes and metadata remains intact.
- [x] 2.2 Re-run `openspec status --change "<name>" --json` and verify `specs` is `skipped`, no specs file exists, and proposal/design/tasks follow the CLI-reported dependency order and resolved output paths; verify status after each artifact.
- [x] 2.3 Exercise or inspect failure boundaries for duplicate names, unresolved/denied paths, unexpected non-skipped specs, and CLI errors; verify planner stops with the exact blocker instead of guessing, bypassing dependencies, or starting `/opsx-apply`.

## 3. Documentation and completion

- [x] 3.1 Add the planned `## [Unreleased]` → `### Changed` entry describing the corrected OpenSpec workflow and its comparison with target `main`; verify no `### Fixed` entry misclassifies this in-progress workflow correction as a released runtime regression.
- [x] 3.2 Run `openspec validate "fix-openspec-skip-specs-workflow" --type change --strict` and `git diff --check`; verify final OpenSpec status is complete with specs skipped, only approved workflow artifacts/guidance are changed, and no build or in-game validation is claimed.
