## Why

OpenSpec is initialized for ECM-R, but `openspec/config.yaml` currently contains only the schema declaration and example comments. As a result, OpenSpec artifact and operation instructions do not receive the project's critical NFSU2, Win-x86, audio, hook, persistence, and validation constraints through a shared project context.

## What Changes

- Add a compact ECM-R project context to `openspec/config.yaml` that points to the authoritative project documents and summarizes compatibility-sensitive runtime invariants.
- Add artifact-specific rules for proposals, specifications, designs, and implementation tasks so planning remains behavior-focused, technically grounded, and verifiable.
- Add advisory operation guidance for `apply` and `archive` covering repository validation, build targets, documentation synchronization, and protected runtime contracts.
- Keep the context as an adapter over `AGENTS.md` and `docs/application-context.md`, rather than duplicating the full application context.

## Capabilities

### New Capabilities

None. This change improves repository workflow guidance and does not add ECM-R runtime behavior.

### Modified Capabilities

None. The change declares `skip_specs: true` because it does not modify a spec-level product requirement.

## Impact

- Affects `openspec/config.yaml` and the project-specific planning inputs returned by OpenSpec.
- Improves the quality of generated proposal, design, task, apply, and archive instructions for ECM-R work.
- Does not change C++ source, runtime behavior, build outputs, deployment filenames, configuration keys, or external dependencies.
- Does not replace the imperative workflow rules in `AGENTS.md` or the architectural source of truth in `docs/application-context.md`.
