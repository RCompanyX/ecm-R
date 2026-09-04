## Why

The ECM-R agents and OpenSpec workflows are available, but their current permissions and routing do not form a reliable end-to-end process. `ecmr-plan` is read-only while the `opsx-*` commands inherit the current agent, so accepting a plan cannot safely create the OpenSpec planning artifacts.

## What Changes

- Keep `ecmr-plan` as the planning agent for feasibility, affected game states, audio transitions, hooks, overlay flows, persistence, risks, validation, and CHANGELOG classification.
- After an explicit user approval of the conversational plan, have `ecmr-plan` run the OpenSpec propose workflow automatically.
- Allow `ecmr-plan` to run the required OpenSpec CLI commands and write only OpenSpec change artifacts; it must remain unable to edit source, runtime, or deployment files.
- Create and validate the OpenSpec planning artifacts in dependency order, using the CLI-provided instructions, context, rules, templates, and dependencies.
- Stop after proposal generation and require a separate explicit user request before `/opsx-apply` can modify implementation files.
- Route the relevant `opsx-*` commands to the agent whose permissions match the operation, especially planning/proposal commands to `ecmr-plan` and implementation commands to `ecmr-dev`.
- Keep implementation, runtime behavior, compatibility-sensitive names, and deployment contracts unchanged by this workflow-only change.

## Scope

- Align `.opencode/agents/ecmr-plan.md`, `.opencode/commands/opsx-*.md`, `opencode.json`, and the corresponding project workflow documentation.
- Preserve the existing `ecmr-explore`, `ecmr-dev`, and `ecmr-release` responsibilities unless required for command routing or approval boundaries.
- Use `skip_specs: true` because this change modifies agent orchestration and repository workflow, not ECM-R product behavior.

## Non-Goals

- Change C++ source code, hooks, audio behavior, overlay behavior, game-state behavior, or runtime configuration.
- Start implementation automatically after OpenSpec artifacts are created.
- Replace `ecmr-plan` with a new planning agent.
- Change the OpenSpec schema, CLI, or global OpenCode configuration.
- Resolve unrelated release-agent wording or command/skill deduplication unless it blocks this workflow.

## Capabilities

### New Capabilities

None. The change is workflow-only and declares `skip_specs: true` in `.openspec.yaml`.

### Modified Capabilities

None. No ECM-R product requirement changes.

## Impact

- Affects OpenCode agent permissions, command routing, and the transition between conversational planning and OpenSpec artifact generation.
- Affects the planning artifacts created under `openspec/changes/` and their validation flow.
- Requires documentation to describe the two approval gates: plan approval creates OpenSpec artifacts, while an explicit apply request permits implementation.
- Does not affect runtime binaries, BASS loading, NFSU2 integration, build outputs, deployment filenames, or user configuration keys.

## Risks

- A permission that is too broad could allow planning to edit project code; restrict writes to OpenSpec paths and validate the resolved agent permissions.
- Automatic proposal generation could be mistaken for implementation approval; require a distinct apply request and stop after planning artifacts are ready.
- Command and skill entry points could drift; route them consistently and validate both entry points against the same OpenSpec behavior.

## CHANGELOG Classification

Classify the workflow integration under `### Added` in `CHANGELOG.md`. The automatic plan-to-OpenSpec transition and scoped artifact-writing capability are absent from the `main` target branch, even though the existing agents and initial OpenSpec commands are present on the working branch.
