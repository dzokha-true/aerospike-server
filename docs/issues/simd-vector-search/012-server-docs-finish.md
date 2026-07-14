---
issue: 12
title: "server docs finish: ADR 0004 follow-up, solutions, phase docs"
state: OPEN
parent: 1
blocked-by: 11
---
Repo: aerospike-server-upstream, branch factory/simd-vector-search.
Finish-boundary docs job — no cold judge; the orchestrator grades the
checkrun directly. Dispatch block will carry the change-context digest.

## What to build

1. `docs/adr/0004-vector-distance-protocol.md`: update the Distance
   implementation section — SIMD follow-up shipped (TU layout, kernel
   table, `AEROSPIKE_VECTOR_SIMD`, CPUID policy, NEON baseline).
2. `docs/agent-phases/README.md` + `phase-4-sptag-integration.md`: mark
   completion state truthfully, pointing at run artifacts.
3. `docs/solutions/<slug>.md` entries from the run's rulings files and job
   reports (nontrivial diagnoses, blocker answers, what-did-not-work).
4. Conformance/benchmark doc pointers consistent (tests README, docker
   README cross-links).

## Boundaries

- MAY TOUCH: `docs/` EXCEPT `docs/checks/`, `docs/issues/`, `docs/runs/`.
- MUST NOT TOUCH: `docs/checks/`, `docs/issues/`, `docs/runs/`, any code.

Check file (read-only): `docs/checks/simd-vector-search/012-server-docs-finish.md`
Report path: `docs/jobs/simd-vector-search/012-server-docs-finish-01.md`

<!-- architect-run: simd-vector-search -->

## Comments
