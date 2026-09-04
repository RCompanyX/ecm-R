## Context

See `proposal.md` for the motivation and scope. The repository has separate ECM-R agents and OpenSpec commands, but OpenCode commands currently inherit the active agent. The planning agents are read-only while OpenSpec proposal generation must create a change and write planning artifacts.

This is a workflow-only change. It must not alter the NFSU2 runtime, audio state machine, hooks, FNG package handling, overlay, hotkeys, persistence, build outputs, or deployment contracts.

## Goals / Non-Goals

**Goals:**

- Keep `ecmr-explore` focused on read-only feasibility research.
- Keep `ecmr-plan` as the planning agent and make it the controlled owner of OpenSpec artifact generation after explicit approval.
- Make the transition from an accepted conversational plan to the OpenSpec propose workflow automatic within the same planning session.
- Preserve two independent approval gates: plan approval permits OpenSpec artifact writes; an explicit apply request permits implementation writes.
- Route each `opsx-*` command to an agent with permissions appropriate to its operation.
- Make the generated OpenSpec context, rules, templates, dependencies, and status checks part of the normal planning flow.

**Non-Goals:**

- Change any ECM-R product requirement or introduce a capability spec.
- Automatically start `/opsx-apply` after proposal generation.
- Allow `ecmr-plan` to edit source, runtime configuration, documentation outside the planning scope, or deployment files.
- Replace the OpenSpec CLI or make its prompt-level context and rules hard enforcement.
- Redesign unrelated release-agent wording or remove all command/skill duplication.

## Decisions

### Keep planning and implementation separate

`ecmr-plan` remains the primary conversational planner. After the user explicitly approves the plan, it runs the equivalent of `/opsx-propose` and then stops. The implementation boundary remains a separate user request handled by `ecmr-dev` and `/opsx-apply`.

This avoids the current automatic plan-to-code delegation and does not depend on nested `Task` calls or increased `subagent_depth`.

### Give the planner scoped OpenSpec access

`ecmr-plan` will retain read-only access to the project and source code. Its additional write capability will be limited to OpenSpec change artifacts, with the OpenSpec CLI available for change creation, status, instructions, and validation. Source and runtime paths remain denied.

The planner will use the artifact graph returned by `openspec status --json`, request instructions for the current artifact, read completed dependencies, apply the returned template and rules, write the resolved artifact, and re-check status before continuing. It will use `skip_specs: true` only for workflow-only changes such as this one.

### Use explicit command routing

The command frontmatter will select the responsible agent instead of relying on whichever agent is active:

- `/opsx-explore` -> `ecmr-explore`.
- `/opsx-propose`, `/opsx-update`, and `/opsx-sync` -> `ecmr-plan`.
- `/opsx-apply` and `/opsx-archive` -> `ecmr-dev`.

The command text and the corresponding OpenSpec skills remain behaviorally aligned. The command route is the user-facing entry point; the skill remains reusable when an agent needs the same procedure internally.

### Model the approval boundary explicitly

The planning state is treated as a small state machine:

```text
PLAN_DRAFTED
    -> USER_APPROVES_PLAN
    -> OPEN_SPEC_CAPTURE
    -> ARTIFACTS_READY
    -> USER_REQUESTS_APPLY
    -> IMPLEMENTATION
```

`USER_APPROVES_PLAN` authorizes only creation of `.openspec.yaml`, `proposal.md`, `design.md`, and `tasks.md` for this workflow-only change, or the equivalent artifact set for a product change. `ARTIFACTS_READY` does not authorize implementation.

### Preserve runtime boundaries

GameFlowState values, playlist context, audio transitions, loading behavior, chyron safety, hooks, and FNG packages are not changed. The design and task validation will explicitly state that these areas are preserved and that no runtime build or in-game claim is made for this workflow-only change.

## Risks / Trade-offs

- [Overbroad planner permission] `ecmr-plan` could edit project code if its permission patterns are too broad. -> Deny all edits first, allow only the resolved OpenSpec artifact roots, and inspect the resolved agent configuration.
- [Approval confusion] Automatic proposal generation could be mistaken for implementation approval. -> Use separate user-facing confirmation text and stop after `ARTIFACTS_READY`.
- [Command inheritance] A command could run under the wrong agent if routing is omitted. -> Set an explicit agent on every relevant command and verify the resolved OpenCode configuration.
- [Instruction drift] Commands and skills can diverge because both contain workflow text. -> Validate both entry points against OpenSpec status, instruction, and validation behavior; leave broader deduplication out of scope.
- [Prompt-level guidance] OpenSpec context and rules are not hard enforcement. -> Treat CLI state and validation as controlling, record instruction failures, and never claim completion from guidance alone.

## Migration Plan

1. Update the agent and command configuration on the working branch without changing ECM-R source or runtime files.
2. Verify the resolved permissions and command-to-agent mapping with OpenCode diagnostics.
3. Run a workflow-only OpenSpec dry run using a temporary or disposable change path if a safe test fixture is available; otherwise validate the active artifacts and CLI responses without claiming an end-to-end implementation test.
4. Run `openspec doctor --json`, `openspec validate --all --strict`, and `git diff --check`.
5. Roll back by reverting the workflow configuration files; no runtime or user configuration migration is required.

## Open Questions

None. The command/skill deduplication question is intentionally deferred because it is not required for the approval and permission boundaries defined here.
