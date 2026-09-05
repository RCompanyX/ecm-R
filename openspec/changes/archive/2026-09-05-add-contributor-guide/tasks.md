## 1. Contributor Guide

- [x] 1.1 Add the root-level `CONTRIBUTING.md` entry point with the NFSU2-only and Win-x86 scope, canonical-document ownership, issue routing, and contribution flow; verify every referenced canonical file and existing issue template path exists.
- [x] 1.2 Document branch, review, approval, and concise AI/OpenSpec workflow guidance without inventing commit or PR conventions; verify the wording matches `AGENTS.md`, `.opencode/`, and `openspec/config.yaml`.
- [x] 1.3 Document local setup, recursive submodules, `generate.bat`, `Release | Win-x86`, CI checks, and the dynamic official `bass.dll` boundary; verify commands, output names, and deployment paths match `BUILDING.md` and `.github/workflows/build.yml`.
- [x] 1.4 Add the runtime safety and validation checklist covering GameFlowState/frontend/loading/racing context, audio pause/loading/chyron and FNG package behavior, hook source paths, overlay/input flows, persisted INI sections, compatibility-sensitive names, licensing, and attribution; verify it links to `docs/application-context.md` and does not claim changed runtime behavior.

## 2. Changelog

- [x] 2.1 Add the contributor-guide entry under `CHANGELOG.md` `## [Unreleased]` / `### Added`; verify it is classified against `main` as a new documentation feature and is not listed under `### Fixed`.

## 3. Documentation Validation

- [x] 3.1 Review Markdown links, commands, filenames, and source-of-truth ownership against the canonical documents, then run `git diff --check`; verify no source, runtime configuration, deployment file, unrelated documentation, or OpenSpec specs file was changed.
- [x] 3.2 Run `openspec validate "add-contributor-guide" --type change --strict`; verify the workflow-only change passes with `specs` skipped and all remaining artifacts complete.
