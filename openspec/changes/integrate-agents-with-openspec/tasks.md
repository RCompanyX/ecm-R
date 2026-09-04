## 1. Planner Permissions and Approval Gates

- [ ] 1.1 Update `ecmr-plan` so an explicit user approval transitions from the conversational plan to the OpenSpec propose workflow, then stops at `ARTIFACTS_READY`; verify the prompt no longer delegates automatically to `ecmr-dev` or implies implementation approval.
- [ ] 1.2 Configure `ecmr-plan` to run the required OpenSpec CLI commands and write only resolved OpenSpec change artifacts while keeping source, runtime, deployment, and unrelated documentation edits denied; verify the resolved permissions with `opencode debug agent ecmr-plan`.
- [ ] 1.3 Keep `ecmr-explore` read-only while allowing the OpenSpec status/context lookups needed to identify active changes; verify it cannot edit files or run implementation commands and can report the current OpenSpec state.

## 2. OpenSpec Command Routing

- [ ] 2.1 Assign explicit agents to the `opsx-*` command entry points: exploration to `ecmr-explore`, proposal/update/sync to `ecmr-plan`, and apply/archive to `ecmr-dev`; verify the resolved command configuration and each command's selected agent.
- [ ] 2.2 Align the command and skill entry points with the same approval boundaries, artifact dependency handling, and status/validation checks; verify both proposal entry points stop after planning artifacts and both apply entry points require an explicit change name or unambiguous active change.
- [ ] 2.3 Preserve the two-step handoff from `ARTIFACTS_READY` to `/opsx-apply` and prevent nested automatic `explore -> plan -> dev` delegation; verify a planning session cannot start implementation without a separate apply request.

## 3. Workflow Validation

- [ ] 3.1 Exercise the workflow with a disposable OpenSpec change or isolated test fixture: create a change, generate its planning artifacts through the accepted-plan transition, and verify `proposal`, `design`, and `tasks` reach `done` while `specs` remains skipped only for workflow-only scope.
- [ ] 3.2 Verify failure handling for denied paths, unavailable OpenSpec commands, invalid instruction output, and incomplete artifacts; confirm the planner stops and reports the blocker without editing implementation files.
- [ ] 3.3 Run `openspec doctor --json`, `openspec validate --all --strict`, OpenCode diagnostics, and `git diff --check`; verify no ECM-R source, runtime, deployment, or user-configuration behavior changed.

## 4. Documentation and Changelog

- [ ] 4.1 Update `AGENTS.md` and `docs/application-context.md` to document the planner-to-OpenSpec transition, the two approval gates, command routing, and the separate `ecmr-dev` apply phase; verify the documented flow matches the resolved configuration.
- [ ] 4.2 Add the workflow integration under `### Added` in `CHANGELOG.md`, with classification based on the feature being absent from the `main` target branch; verify the entry remains under `## [Unreleased]` and describes no runtime behavior change.
- [ ] 4.3 Review the final diff and verify only the intended agent, command, OpenCode workflow documentation, and changelog files changed; preserve runtime filenames, deployment paths, attribution, and the existing OpenSpec schema.
