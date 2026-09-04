## 1. Create third-party notice

- [x] 1.1 Verify `@fission-ai/openspec` version `1.12.0`, MIT declaration, upstream URL, copyright holder, and license text against the reviewed package metadata and `LICENSE`; completion is confirmed when all notice values match exactly.
- [x] 1.2 Add root-level `THIRD_PARTY_NOTICES.md` covering modified `.opencode/commands/opsx-*.md` files, with OpenSpec provenance and the complete MIT text; verify the file exists at repository root and clearly separates OpenSpec terms from ECM-R's `LICENSE` and `Copyright (c) 2022 BttrDrgn`.

## 2. Record and validate documentation change

- [x] 2.1 Add the notice entry under `CHANGELOG.md` → `## [Unreleased]` → `### Added`; verify classification against base branch `main` and confirm no `### Changed` or `### Fixed` entry is introduced.
- [x] 2.2 Review changed paths to confirm only the intended notice, changelog, and OpenSpec planning artifacts are involved; run `openspec validate "add-openspec-third-party-notices" --type change --strict` and `git diff --check`, with both validations reporting success.
