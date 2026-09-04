## 1. Configure the shared OpenSpec context

- [x] 1.1 Replace the example-only contents of `openspec/config.yaml` with a compact ECM-R project context covering NFSU2/Win-x86 scope, runtime compatibility, audio and lifecycle invariants, canonical source paths, and validation boundaries; verify `openspec doctor --json` reports a healthy root.
- [x] 1.2 Add artifact-specific rules for `proposal`, `specs`, `design`, and `tasks` without using operation IDs as rule keys; verify the corresponding `openspec instructions <artifact> --change "improve-openspec-context" --json` responses expose the intended rules without unknown-artifact warnings.
- [x] 1.3 Add advisory `apply` and `archive` guidance for canonical-document review, scoped Win-x86 validation, protected runtime contracts, CHANGELOG handling, and unapproved Git writes; verify `openspec instructions apply --change "improve-openspec-context" --json` and `openspec instructions archive --change "improve-openspec-context" --json` expose the intended inputs without changing CLI-controlled state or paths.

## 2. Validate and record the workflow improvement

- [x] 2.1 Run `openspec validate --all --strict` and verify the change and repository specs report no validation errors, with the `specs` artifact remaining deliberately skipped by `skip_specs: true`.
- [x] 2.2 Add an `### Added` entry under `## [Unreleased]` in `CHANGELOG.md` for the ECM-R-aware OpenSpec context and guidance, and verify its classification is based on the feature being absent from the `main` target branch.
- [x] 2.3 Review the final diff and run `git diff --check`; verify that only the intended OpenSpec configuration and changelog files changed and that no ECM-R source, runtime, deployment, or user-configuration behavior was modified.
