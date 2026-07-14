---
issue: 9
title: "sptag B3: SPEC-4-INTEG-001 end-to-end top-K parity (offload vs baseline)"
state: OPEN
parent: 1
blocked-by: 6, 7, 8
---
Repo: **SPTAG-upstream**, branch factory/simd-vector-search.

## What to build

The Phase-4 gate: prove the offload path returns the same answers as the
baseline client path against a real server.

1. `tests/integration/offload/` in SPTAG-upstream: compose file + `run.sh`
   orchestrating (a) one asd container from image `as-vector:check`
   (built by issue #8's `docker/Dockerfile` in the server repo; `run.sh`
   accepts `SERVER_IMAGE` env, default `as-vector:check`, and
   `SERVER_REPO` env pointing at the server repo to build it when absent —
   default `../aerospike-server-upstream`), (b) one SPTAG container from the
   #6 image.
2. Inside the SPTAG container: build a small SPANN index with
   `Storage=AEROSPIKEIO` — synthetic seeded dataset, ~5000 vectors, dim 64,
   float32, L2 (must match the server namespace vector config) — then run
   the same 100 seeded queries (K=10) twice: `SPTAG_AS_VECTOR_DISTANCE=0`
   (baseline MultiGet + ComputeDistance) and `=1` (offload).
3. Compare per-query top-K: mean recall/overlap >= 0.99, ties (equal
   distances) allowed to reorder; print per-query worst overlap. On success
   print exactly `PARITY_OK overlap=<mean>`; nonzero exit otherwise.
4. Failures must dump both result lists for the worst query (diagnosable
   evidence, no silent retry).

## Acceptance criteria

- `bash tests/integration/offload/run.sh` prints PARITY_OK on this host.
- The offload leg demonstrably used VECTOR_DISTANCE (assert via SPTAG log
  line or server-side statistics, not assumption).

## Boundaries

- MAY TOUCH (SPTAG-upstream only): `tests/integration/offload/` (new),
  `Script_AE/` helpers, `Dockerfile` ONLY to add missing runtime tooling
  (record it), `docs/contracts/`.
- MUST NOT TOUCH: `docs/checks/`, `AnnService/` implementation,
  `Test/src/`, server repo, client repo.
- Out of scope: performance measurement (#11), SIMD assertions (#10 covers
  wire-level SIMD equivalence).

Check file (read-only, in SPTAG-upstream):
`docs/checks/simd-vector-search/009-sptag-integ-parity.md`
Report path (in SPTAG-upstream):
`docs/jobs/simd-vector-search/009-sptag-integ-parity-01.md`
Duration hint: index build + two query passes ~15-30m.

<!-- architect-run: simd-vector-search -->

## Comments
