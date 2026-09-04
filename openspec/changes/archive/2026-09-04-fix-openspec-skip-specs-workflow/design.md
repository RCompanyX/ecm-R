## Context

See `proposal.md` for motivation. The repository uses OpenSpec's default
`spec-driven` schema, whose change scaffold includes a specs artifact. In
OpenSpec 1.12.0, `new change` accepts creation metadata but not
`--skip-specs`; `archive` owns that CLI option. The CLI-created
`.openspec.yaml` is therefore the correct place to declare that a
workflow-only change has no spec delta.

Tracked workflow guidance is distributed across `.opencode/agents/`,
`.opencode/commands/`, `AGENTS.md`, `docs/application-context.md`, and
`openspec/config.yaml`. The installed OpenSpec package and ignored generated
skills are external/generated inputs, not edit targets.

## Goals / Non-Goals

**Goals:**

- Make workflow-only proposal capture compatible with OpenSpec 1.12.0.
- Make command capability and metadata timing explicit to planner agents.
- Use CLI-resolved paths, status, instructions, and dependency edges as the
  workflow authority.
- Preserve metadata already written by `openspec new change`.
- Fail closed when creation, path resolution, or skip-state verification fails.

**Non-Goals:**

- Change OpenSpec, its installed package, schema definitions, or generated
  ignored skills.
- Add or modify ECM-R product requirements, runtime code, build/deployment
  configuration, or user INI settings.
- Apply the change or delegate implementation to `ecmr-dev`.

## Decisions

### Scaffold first; declare skip state second

Use `openspec new change "<name>"` without `--skip-specs`. After successful
scaffolding, read `openspec status --change "<name>" --json`, take its
`changeRoot`, and add only `skip_specs: true` to that CLI-created
`.openspec.yaml` for workflow-only scope. Preserve `schema`, `created`, store
context, and any other existing metadata.

**Alternative rejected:** passing `--skip-specs` to `new change`. OpenSpec
1.12.0 rejects it as an unknown option. **Alternative rejected:** changing the
default schema to avoid specs. That would alter project-wide workflow semantics
and hide the explicit no-spec decision.

### Let CLI status drive paths and dependency order

Re-run status after metadata update and require the specs artifact to report
`skipped`. Do not create a specs file. For each remaining required artifact,
request `openspec instructions <artifact-id> --change "<name>" --json`, read
completed dependencies from disk, use the returned template/rules/context, and
write only the returned `resolvedOutputPath`. Re-run status after every write.

**Alternative rejected:** assuming `openspec/changes/<name>` or a fixed
proposal/design/tasks order without status. Resolved roots, stores, optional
artifacts, and dependency edges are CLI-controlled.

### Apply skip marker only to workflow-only scope

Planner guidance must distinguish changes with no spec-level behavior from
product changes. Workflow, tooling, and documentation-only changes use the
marker after scaffolding; product behavior changes retain the specs artifact.
This prevents `skip_specs` from masking missing requirements.

### Preserve approval and failure boundaries

Preflight for an existing change name and ask before continuing it. Stop on
creation failure, missing/denied resolved paths, unexpected status, or failed
validation; report the exact blocker. Artifact readiness ends planning and does
not authorize `/opsx-apply`, implementation, commits, or runtime validation.

### Runtime boundary remains unchanged

No NFSU2 runtime state participates in this workflow correction:

- `GameFlowState`: none of `None`, loading, frontend, racing, unloading, or
  exit states change.
- Audio: no playlist context, transition, loading stop, pause/resume, chyron,
  BASS, or FNG mute behavior changes.
- Hooks: no `main.cpp`, renderer, package-load, or game-loop hook changes.
- Overlay/input: no menu, hotkey, capture, or localization behavior changes.
- Persistence: no `ecm-r.x86.ini` or `[config]`, `[keys]`, or `[trax]` changes;
  only OpenSpec change metadata records `skip_specs`.

## Risks / Trade-offs

- **CLI version drift** → Keep guidance tied to verified OpenSpec 1.12.0
  behavior; use status/instructions output and stop when output differs.
- **Wrong metadata location** → Use `changeRoot` from status, never a guessed
  path; verify existing metadata before and after the single-field update.
- **Skipped specs hiding product behavior** → Require explicit workflow-only
  classification and status `skipped`; leave product changes spec-driven.
- **Duplicate or partial changes** → Preflight with `openspec list/status`, ask
  before continuation, and stop on incomplete or blocked status.
- **Prompt divergence** → Keep planner agent, proposal command, AGENTS.md, and
  application context aligned; leave installed/generated copies untouched.

## Migration Plan

1. Update tracked workflow guidance with the supported scaffold, metadata, and
   verification sequence.
2. Validate this change through OpenSpec status, instructions, and final
   validation; use `git diff --check` for tracked text edits.
3. Rollback requires only reverting those guidance/documentation edits. No
   runtime migration, save-data migration, deployment change, or BASS handling
   change is required.
