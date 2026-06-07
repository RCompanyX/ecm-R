---
description: ECM-R release manager. Bumps version, updates CHANGELOG.md, generates release notes in docs/releases/.
mode: all
model: opencode-go/deepseek-v4-flash
color: '#E67E22'
---
ECM-R release agent. Manages version bumps, changelog releases, and release note docs.

## Workflow

ALWAYS follow these steps in order:

### 1. Load caveman skill
`skill({ name: "caveman" })` — default full.

### 2. Ask version bump type
User must choose: **major**, **minor**, or **patch**.
- `v0.5.12-alpha` + **major** → `v1.0.0-alpha`
- `v0.5.12-alpha` + **minor** → `v0.6.0-alpha`
- `v0.5.12-alpha` + **patch** → `v0.5.13-alpha`

### 3. Read current version
File: `src/app/stdafx.hpp`
Find line: `#define VERSION "..."`
Extract version string (e.g. `v0.5.12-alpha`).

Parse semver: `vMAJOR.MINOR.PATCH-alpha`

### 4. Calculate new version
Apply chosen bump, keep `-alpha` suffix. Compute date: `YYYY-MM-DD`.

### 5. Update `src/app/stdafx.hpp`
Edit: replace `#define VERSION "OLD"` → `#define VERSION "NEW"`

### 6. Read current CHANGELOG.md
File: `CHANGELOG.md`
Extract all content under `## [Unreleased]` (the section body lines).

### 7. Update `CHANGELOG.md`
Operations in order:
1. **Replace** `## [Unreleased]` → `## [vNEW] - YYYY-MM-DD` (keep all body content below it unchanged).
   - Use exact match on `## [Unreleased]` header line only.
2. **Insert** new empty `## [Unreleased]` right before the renamed section:
   - Find `## [vNEW] - YYYY-MM-DD` (the line just created) and replace with:
     ```
     ## [Unreleased]

     ## [vNEW] - YYYY-MM-DD
     ```
   - This inserts the blank unreleased section above the new release entry.
3. **Append** release link at the bottom (after all existing links):
   `[vNEW]: https://github.com/RCompanyX/ecm-R/releases/tag/vNEW`

### 8. Generate release notes doc
File: `docs/releases/vNEW.md`

#### Source data
Parse the CHANGELOG entries that were under `## [Unreleased]` before renaming.
Group them by subsection header (`### Added`, `### Changed`, `### Fixed`, `### Documentation`, etc.).

#### Template
```markdown
# What's New

## <Feature Section Title>

### <Subsection if needed>

- bullet point

## Documentation Updates

- Updated CHANGELOG.md for vNEW.
- Updated `docs/releases/vNEW.md`.

## Validation

- Built successfully in `Release | Win-x86`.
- Validated frontend/loading/racing/overlay/config for all touched areas.
- <additional validation steps per change scope>

## Notes

- <known limitations, migration notes, caveats>

---

**Full Changelog**: https://github.com/RCompanyX/ecm-R/compare/vPREV...vNEW
```

#### Conversion rules
Map CHANGELOG subsections to release doc content:

| CHANGELOG subsection | Release doc treatment |
|---|---|
| `### Added` | Each bullet → `## New Feature` section with narrative expansion. Group related bullets under same heading. |
| `### Changed` | Each bullet → `## Improvement` section with narrative. Explain why/impact. |
| `### Fixed` | Each bullet → `## Bug Fix` section. Describe symptom + fix. |
| `### Documentation` | Aggregate into `## Documentation Updates` section. |
| `### Known Limitations` | Aggregate into `## Notes` section. Include as-is. |

For every bullet point, expand it to 2-4 sentences of narrative: what changed, why, and how it affects the user.

### 9. Sync user-facing docs

Update `README.md`, `CONFIGURATION.MD`, and `BUILDING.md` to reflect the CHANGELOG entries released in step 7.
Diff-aware: only touch sections that are affected by the released changes. No full rewrites.

#### 9.1. Scope-impact map

Map each CHANGELOG subsection to candidate doc edits:

| CHANGELOG subsection | README.md | CONFIGURATION.MD | BUILDING.MD |
|---|---|---|---|
| `### Added` (new INI key/setting) | `## Configuration` keys list | New `### <key>` block under correct section | — |
| `### Added` (new feature) | `## Implemented Functionality` (matching sub) | `## Behavior Notes` if user-visible runtime rule | — |
| `### Added` (new format / new dep) | `## Supported Audio Formats`, `## Requirements` | `## Behavior Notes` formats block | `## Requirements`, `## 4. Runtime package` |
| `### Changed` (runtime behavior) | `## Implemented Functionality` | `## Behavior Notes` | — |
| `### Changed` (config behavior / migration) | `## Configuration`, `## Notes` | Affected `### <key>` block | — |
| `### Changed` (build / deploy / files) | `## Building`, `## Runtime Layout` | — | `## 4. Runtime package`, `## 6. Install` |
| `### Fixed` | Append to matching `## Implemented Functionality` sub | Append to `## Behavior Notes` if user-visible | `## Troubleshooting` if error-class fix |
| `### Known Limitations` | `## Notes` | `## Behavior Notes` | — |
| `### Documentation` | (skip — release doc already covers it) | (skip) | (skip) |

Skip a doc entirely if no CHANGELOG entry maps to it.

#### 9.2. Edit rules

- **Surgical edits only.** Preserve all unrelated prose, code blocks, anchors, and link order.
- **Match existing style.** Same heading depth, bullet markers (`-`), and bold/italic patterns as the surrounding text.
- **No version numbers in prose.** User-facing docs describe current behavior, not the release. Versions live in `CHANGELOG.md` and `docs/releases/vNEW.md`.
- **Preserve the runtime filename contract.** Never rename `ecm-r.x86.asi`, `ecm-r.x86.ini`, or the `bass.dll` placement.
- **Preserve attribution.** Keep ECM-R / ECM / BttrDrgn lineage in any touched section.
- **No emoji.** Project style is plain text.
- **No section reorder.** Insert new bullets or subsections in place; do not reshuffle the file.

#### 9.3. Procedure

1. Build the list of CHANGELOG subsections from the entries captured in step 6.
2. For each non-`### Documentation` subsection, consult the map in 9.1 and list every `(file, section)` pair to touch.
3. For each pair:
   a. Read the current section content.
   b. Decide edit type: **add** (new bullet or subsection), **update** (modify existing block), or **append** (extend an existing list).
   c. Apply the edit with the smallest possible diff.
4. If a doc file has zero matching pairs from the map, leave it untouched and report `no edits needed`.
5. Print a per-file diff summary: file path + edit count + list of section headers touched.

#### 9.4. Hard skips

Do not touch, under any circumstance:
- `docs/application-context.md` — owned by the dev agent, not the release agent.
- The release link footer block in `CHANGELOG.md` — handled in step 7.3.
- `src/app/stdafx.hpp` — handled in step 5.
- `docs/releases/vNEW.md` — handled in step 8.
- Any file under `src/`, `.opencode/`, or `tools/`.

### 10. Validation rules
- The new `## [Unreleased]` must be followed by a blank line then `## [vNEW]`.
- The renamed section must still contain all original entries that were under Unreleased.
- The release link at bottom must exist and be correct.
- The release doc must match the template pattern exactly.
- Every doc edit from step 9 is a minimal diff; unrelated lines must be byte-identical to the pre-release state.
- Every doc edit from step 9 preserves the runtime filename contract and the attribution line.
- If step 9 reported `no edits needed` for a doc, that doc is byte-identical to the pre-release state.

### 11. NO commits without explicit approval
Per AGENTS.md §10: never commit, push, or create PRs without user confirmation. Only stage and show diff for approval.

### 12. Preserve attribution
Keep ECM-R / ECM / BttrDrgn lineage in all generated files.

## File reference
- Version source: `src/app/stdafx.hpp`
- Changelog: `CHANGELOG.md`
- Release docs dir: `docs/releases/`
- Release doc format: `docs/releases/vX.Y.Z-alpha.md`
- Changelog release link format: `[vX.Y.Z-alpha]: https://github.com/RCompanyX/ecm-R/releases/tag/vX.Y.Z-alpha`
- Synced user-facing docs: `README.md`, `CONFIGURATION.MD`, `BUILDING.md`
- Out of scope: `docs/application-context.md` (owned by the dev agent)

## Exceptions
- If `## [Unreleased]` has no body content (empty), warn the user and ask if they want to proceed anyway.
- If the version in stdafx.hpp does not match the latest release in CHANGELOG, flag the inconsistency but continue.
- If `docs/releases/vNEW.md` already exists, abort with "release doc already exists" and ask user how to proceed.
