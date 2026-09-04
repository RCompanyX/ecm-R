## Context

The proposal addresses an empty project-context boundary in OpenSpec; see `proposal.md` for the motivation. The repository already has two canonical information layers: `AGENTS.md` for imperative workflow and policy, and `docs/application-context.md` for stable ECM-R architecture and runtime behavior. `opencode.json` supplies those documents to OpenCode, but the OpenSpec CLI only receives project-specific context from `openspec/config.yaml`.

OpenSpec 1.12 injects the config `context` into artifact instructions and into `apply`/`archive` inputs. It applies `rules` by artifact ID and exposes operation guidance only for `apply` and `archive`. The config parser limits context to 50 KB, so copying the full application context would be valid in size but inefficient and prone to drift.

## Goals / Non-Goals

**Goals:**

- Make OpenSpec-generated planning and operation instructions ECM-R-aware without requiring a separate prompt-specific copy of the project documentation.
- Summarize stable compatibility, lifecycle, playback, persistence, and validation invariants in a compact shared context.
- Keep artifact output behavior distinct: proposals cover intent, specs cover observable behavior, designs cover technical approach, and tasks cover executable verification.
- Give `apply` and `archive` project-specific advisory guidance while preserving the CLI's state, path, dependency, and completion rules.
- Keep the configuration valid for the built-in `spec-driven` schema and easy to review when project behavior changes.

**Non-Goals:**

- Change ECM-R source code, runtime behavior, hooks, build outputs, deployment names, or external dependencies.
- Replace `AGENTS.md` or `docs/application-context.md` as the authoritative project documents.
- Create main or delta product specifications for audio, overlay, game integration, or settings.
- Modify OpenCode agents, slash commands, schemas, or the OpenSpec CLI.
- Encode volatile version, branch, release-date, or hook-address details in the shared context.

## Decisions

### Use one compact project adapter

`openspec/config.yaml` will contain the `context`, `rules`, and `operations` fields supported by the installed OpenSpec CLI. The context will identify ECM-R's scope, toolchain, runtime dependency boundary, compatibility-sensitive filenames, lifecycle constraints, and source-of-truth paths. It will point agents to the detailed documents and code rather than reproducing them.

Duplicating all of `docs/application-context.md` was rejected because it would increase every artifact prompt and create a second document that could diverge from the canonical architecture description. Changing only `.opencode/agents/` was also rejected because it would not help other OpenSpec consumers and would duplicate guidance already injected by `opencode.json`.

### Keep rules artifact-specific

Rules will use the built-in artifact IDs `proposal`, `specs`, `design`, and `tasks`:

- `proposal` rules will require explicit scope, non-goals, affected areas, and CHANGELOG classification against the target branch, normally `main`.
- `specs` rules will require observable behavior and testable `WHEN`/`THEN` scenarios, while excluding internal APIs, hook addresses, and implementation choices.
- `design` rules will require relevant mappings between `GameFlowState`, playlist context, audio transitions, loading screens, chyron, hooks/FNG packages, overlay or hotkeys, persistence, and failure handling.
- `tasks` rules will require dependency-ordered checkbox tasks with concrete verification, relevant integration coverage, documentation updates, and CHANGELOG handling.

Rules will constrain artifact content but will not be copied into the generated artifacts. They will not use `apply` or `archive` as rule keys because those are operation IDs, not artifact IDs.

### Keep operation guidance advisory

`operations.apply.guidance` will remind implementers to read the canonical project documents, inspect the relevant source, use `generate.bat` and the `Release | Win-x86` target, validate affected runtime flows, update `CHANGELOG.md`, and avoid unapproved Git writes.

`operations.archive.guidance` will remind the archiver to validate the planning state, preserve runtime and attribution contracts, and leave version bumps and release-note generation to `ecmr-release`. The guidance must not replace CLI-controlled checks, choose paths, bypass incomplete tasks, or force spec synchronization.

### Deliberately skip product specs

The change metadata will retain `skip_specs: true`. This is appropriate because the change modifies workflow configuration and does not introduce or change an ECM-R user-visible requirement. The `specs` artifact remains skipped rather than receiving an invented tooling requirement.

### Prefer stable facts over volatile implementation details

The shared context will include stable boundaries such as NFSU2-only scope, Win-x86 validation, dynamic BASS loading, renderer-dependent initialization, pause/context invariants, and compatibility-sensitive runtime names. It will direct agents to verify hook addresses and detailed transitions in source files instead of embedding values that can become stale.

## Risks / Trade-offs

- [Context drift] A summarized invariant can become stale when runtime behavior changes. -> Keep canonical-source pointers in the context and update this adapter in the same planning/configuration change when the invariant changes.
- [Prompt bloat] Excessive detail would be repeated in every artifact and operation instruction. -> Keep `context` compact and move phase-specific detail into `rules` or operation guidance.
- [Overconstraint] A rule may prevent a valid future change or make a documentation-only task appear to require a build. -> Phrase only genuine project invariants as constraints and qualify validation guidance by affected scope.
- [Schema mismatch] A rule key outside the active artifact graph may be ignored or warned about. -> Use only the current built-in IDs and validate the config after editing.
- [Guidance conflict] Advisory guidance could be mistaken for a CLI completion requirement. -> Keep guidance explicitly advisory and preserve OpenSpec's status, dependency, path, and archive behavior.

## Migration Plan

Replace the example-only contents of `openspec/config.yaml` with the approved project context, artifact rules, and operation guidance. Validate the resulting configuration through OpenSpec health and instruction commands, then review the generated planning inputs before using the configuration for a real change.

Rollback is a single-file restoration of the prior `openspec/config.yaml`; no runtime migration or user configuration migration is required.
