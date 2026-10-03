# Agent instructions

These instructions apply to all work in this repository.

## Authority and read order

Before editing, read:

1. `AGENTS.md`
2. `RAG.md`
3. `docs/00-PROGRAMME.md`
4. `docs/01-ARCHITECTURE.md`
5. `docs/02-VALIDATION.md`
6. `docs/03-EXECUTION-PROTOCOL.md`
7. `docs/04-REFERENCES.md`
8. the complete body and latest comments of the issue being executed

The issue body defines the bounded task. Repository documentation defines cross-cutting architecture, validation and workflow requirements. If live repository state has advanced, reconcile it rather than blindly following stale hashes or assumptions.

## Work discipline

- Reconcile live `main`, the selected issue, relevant open/closed PRs, branches, recent CI, and any handoff comments before editing.
- Resume legitimate existing work for the selected issue instead of duplicating it.
- One issue owns one bounded change. Do not start successor issues during the same run unless the issue explicitly says to.
- Branch from current `main`. Do not force-push `main`.
- Do not modify repository security settings, branch protection, Actions permissions, secrets, or other administrative settings unless a dedicated issue explicitly requires it.
- Preserve unrelated work. Do not make opportunistic refactors that materially broaden scope.
- Prefer upstream public APIs and small local adapters over copying/forking third-party stacks.
- Pin external dependencies to deliberate versions/commits; do not silently float production builds on upstream `master`/latest.
- Do not claim hardware behaviour that was not actually observed. Hardware evidence must identify firmware commit, board, host OS, procedure and result.
- Wi-Fi coexistence, USB composite debug interfaces and Bluetooth SCO/ISO audio are out of MVP scope unless a later issue explicitly activates them.

## Validation and merge

Every implementation PR must:

- build reproducibly from a clean checkout;
- run the strongest relevant automated checks available for the issue;
- include focused regression tests for packet framing/state-machine logic where practical;
- keep documentation/contracts synchronized with implementation changes;
- explain what was tested and what was not;
- avoid suppressing or weakening tests simply to make CI green.

Inspect failing CI logs and repair failures caused by the change. Merge only when:

1. the issue acceptance criteria are genuinely satisfied;
2. all required automated checks are green;
3. any explicitly required hardware evidence exists;
4. the PR is mergeable and no newer conflicting authority has appeared.

After merge, verify the intended commit is on `main` and that required post-merge checks are healthy before closing the issue.

If physical hardware is an explicit acceptance gate and is unavailable, leave a precise evidence-ready handoff and do **not** merge or close the issue on inference alone.

## Commit and PR hygiene

Use descriptive branches and commits. PR bodies should include:

- owning issue;
- concise implementation summary;
- validation performed;
- hardware evidence, when required;
- known limitations/non-goals;
- exact remaining work, if any.

Do not use completion language when a required acceptance gate is still outstanding.
