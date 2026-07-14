---
issue: 13
title: "sptag docs finish: ADR 0002 sync/guard notes, glossary, contracts"
state: CLOSED
parent: 1
blocked-by: 11
---
Repo: **SPTAG-upstream**, branch factory/simd-vector-search.
Finish-boundary docs job — no cold judge; the orchestrator grades the
checkrun directly. Dispatch block will carry the change-context digest.

## What to build

1. `docs/adr/0002-server-side-vector-distance-offload.md`: record the
   quantizer guard and the V1 synchronous-call decision (async request
   param unused) as consequences.
2. `CONTEXT.md` glossary: add/adjust terms that shipped (kernel table /
   vector-simd mode if referenced from SPTAG docs).
3. `docs/contracts/aerospike-kv-backend.md`: reflect the verified build
   path (forked client in Docker, probe marker) and integration/benchmark
   entry points.
4. `docs/solutions/<slug>.md` entries from SPTAG-lane rulings and reports.

## Boundaries

- MAY TOUCH (SPTAG-upstream): `docs/` except `docs/checks/`, `CONTEXT.md`.
- MUST NOT TOUCH: `docs/checks/`, any code, `Dockerfile`, tests.

Check file (read-only, in SPTAG-upstream):
`docs/checks/simd-vector-search/013-sptag-docs-finish.md`
Report path (in SPTAG-upstream):
`docs/jobs/simd-vector-search/013-sptag-docs-finish-01.md`

<!-- architect-run: simd-vector-search -->

## Comments
- 2026-07-14T23:55Z [orchestrator] Docs landed: ADR 0002 V1-synchronous + verification consequences and quantizer refusal; CONTEXT.md kernel-table glossary entry; contract doc verified-build/harness entry points; solutions note (arm64 port + client modules path). Orchestrator-graded per the finish-boundary exception.
- 2026-07-14T23:55Z [orchestrator] VERDICT: PASS (orchestrator-graded).
