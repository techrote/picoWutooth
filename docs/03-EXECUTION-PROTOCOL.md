# Execution protocol

This document defines how each implementation issue is carried from live repository state to an accepted merge.

## 1. Reconcile before editing

For the selected issue:

- fetch current `main`;
- read the complete issue body and latest comments;
- inspect relevant open/closed PRs;
- inspect branches or commits clearly associated with the issue;
- inspect recent required CI;
- read the mandatory corpus listed in `RAG.md`.

If an existing legitimate branch/PR already owns the issue, resume it. Do not create competing work merely because a prompt named an older baseline.

## 2. State the issue boundary

Before substantial changes, identify:

- what the issue owns;
- explicit dependencies/gates;
- acceptance criteria;
- hardware requirement, if any;
- non-goals.

Do not silently absorb work from a successor issue.

## 3. Branch and implement

Create or resume one descriptive branch.

Implementation rules:

- keep third-party changes minimal;
- place project-specific adaptation behind local interfaces;
- add focused tests with the bug/contract, not afterthought tests that merely mirror implementation;
- update authority docs when a contract changes;
- retain useful diagnostics and remove temporary hacks before merge;
- never substitute a mock PASS for an explicitly required physical test.

## 4. Validate locally/hosted

Run the strongest available checks relevant to the issue.

At minimum for ordinary firmware issues:

- configure/build from a clean state;
- native tests;
- any descriptor/contract tests;
- project lint/static checks that are required by CI.

For hardware-gated issues, perform and preserve the exact evidence defined by `docs/02-VALIDATION.md`.

## 5. Open or update the PR

PR body must reference the owning issue and contain:

- summary;
- architecture/behaviour change;
- tests run;
- CI state;
- hardware evidence and exact commit where applicable;
- limitations/non-goals;
- follow-up items that belong to later issues.

Do not mark a PR ready if known required acceptance remains unavailable.

## 6. CI repair loop

After push:

1. inspect every required automated check;
2. inspect logs for failures;
3. distinguish task-caused failures from unrelated infrastructure failure;
4. fix task-caused failures;
5. rerun/refresh until required checks are green.

Do not disable, skip or weaken a meaningful test simply to merge.

If infrastructure is genuinely unavailable, document it and stop rather than representing the gate as passed.

## 7. Merge

Merge only when:

- acceptance criteria are satisfied;
- required automated checks are green;
- required hardware evidence exists;
- branch is current enough to merge safely;
- no blocking review finding remains.

Use the repository's enabled merge method; prefer squash for a bounded issue unless preserving commit structure is materially useful.

## 8. Verify after merge

After merge:

- fetch/reconcile live `main`;
- confirm the merged change is present;
- verify required post-merge CI;
- update/close the issue only when the accepted state is real;
- leave a short final issue comment with merge commit and acceptance evidence.

Do not begin the next issue as part of post-merge verification.

## Hardware-unavailable handoff

If the issue explicitly requires physical hardware and the executor cannot access it:

- complete all non-hardware work that remains useful;
- keep the branch/PR in a truthful state;
- record exact candidate commit/UF2 checksum;
- provide exact commands and expected evidence;
- identify the single remaining gate;
- do not merge/close unless the issue explicitly defines hardware as optional.

## Recovery from interrupted work

When taking over an interrupted issue:

- treat later conservative handoff comments as stronger than optimistic intermediate claims;
- verify claimed CI/evidence from source rather than relying on prose;
- leave broken/forensic branches intact unless the issue explicitly calls for cleanup;
- create a clean recovery branch from accepted `main` when existing work is too confused to safely continue;
- preserve useful commits by cherry-picking only after understanding them.

## Completion language

Use precise states:

- **implemented** — code exists;
- **CI-green** — required automated checks passed for exact commit;
- **hardware-tested** — physical evidence exists for exact commit;
- **accepted/complete** — all issue gates passed and work is merged.

Do not collapse these into one claim.
