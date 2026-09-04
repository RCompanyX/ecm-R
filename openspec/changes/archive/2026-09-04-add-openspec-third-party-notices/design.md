## Context

See `proposal.md` - Why. The repository has a separate ECM-R MIT `LICENSE` and modified tracked OpenSpec command files under `.opencode/commands/opsx-*.md`, but no centralized OpenSpec notice. The reviewed `@fission-ai/openspec` package is version `1.12.0`, declares MIT licensing, and supplies the requested copyright and license text.

## Goals / Non-Goals

**Goals:**

- Place one root-level notice covering the identified OpenSpec-derived command files.
- Preserve upstream name, version, URL, copyright, and complete MIT terms verbatim.
- Keep OpenSpec attribution distinct from ECM-R, ECM, and BttrDrgn attribution.
- Record the addition under the `main`-based Unreleased changelog policy.

**Non-Goals:**

- No source, runtime, build, deployment, configuration, translation, or packaging changes.
- No OpenSpec capability or product requirement; do not create a specs file.
- No license headers in individual command files and no claim that OpenSpec authored ECM-R modifications.
- No notice expansion to ignored/generated `.opencode/skills/` material unless its tracking scope changes later.

## Decisions

1. **Use one root notice instead of per-file headers.** A centralized `THIRD_PARTY_NOTICES.md` avoids duplicating the same MIT text while clearly naming the covered `.opencode/commands/opsx-*.md` files.
   - **Alternative:** Add headers to every command file. Rejected because it broadens the edit set and mixes upstream notice text into ECM-R-maintained command content.

2. **Pin notice metadata to the reviewed package.** Document `@fission-ai/openspec` `1.12.0`, `https://github.com/Fission-AI/OpenSpec`, `Copyright (c) 2024 OpenSpec Contributors`, and copy the installed package's complete MIT license text verbatim.
   - **Alternative:** Use an unversioned or abbreviated notice. Rejected because provenance and exact attribution would be less auditable.

3. **Keep project licensing separate.** Leave root `LICENSE` (`Copyright (c) 2022 BttrDrgn`) unchanged; state that OpenSpec terms apply only to covered derived material and do not relicense ECM-R work.

4. **Treat runtime impact as empty.** No `GameFlowState` values, playlist-context mapping, loading-screen stop behavior, audio transitions, pause/resume, chyron safety, hooks/FNG packages, overlay/hotkeys, or persisted settings change. No BASS or renderer path is involved.

5. **Skip product specs.** Retain `skip_specs: true`; this is documentation/compliance work with no observable product requirement. Proposal, design, and tasks remain the required planning artifacts.

6. **Classify changelog addition as `### Added`.** The notice is absent from target branch `main`, so the Unreleased entry describes new documentation, not a behavior change or regression fix.

## Risks / Trade-offs

- [Incorrect upstream text or metadata] → Verify package `1.12.0` metadata and copy its full MIT text without edits before final validation.
- [Ambiguous ownership scope] → Explicitly separate covered OpenSpec-derived commands from ECM-R-authored modifications and retain the existing ECM-R `LICENSE`.
- [Notice omitted from a future binary-only archive] → Keep this change repository/documentation-only; release packaging decisions remain outside this change and must not alter runtime deployment contracts.
- [Accidental runtime coupling] → Limit edits to root notice, Unreleased changelog content, and OpenSpec planning artifacts; run documentation/OpenSpec validation only.

## Migration Plan

Add the notice and Unreleased entry, then validate OpenSpec artifacts and Markdown whitespace. Rollback requires removing only the new notice and its changelog entry; no runtime migration or config repair is needed.
