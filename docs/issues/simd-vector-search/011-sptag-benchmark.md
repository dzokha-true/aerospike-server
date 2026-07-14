---
issue: 11
title: "C2: 3-node benchmark — baseline vs offload-scalar vs offload-NEON"
state: CLOSED
parent: 1
blocked-by: 3, 9, 10
---
Repo: **SPTAG-upstream**, branch factory/simd-vector-search.

## What to build

1. `tests/benchmark/offload/` in SPTAG-upstream: compose using the server
   repo's `docker/compose-cluster3.yml` image (3 asd nodes, arm64 native)
   plus the SPTAG client container. `run.sh [quick|full]`:
   - Dataset: synthetic seeded, `full` = 100k vectors dim 768 float32 L2,
     `quick` = 10k vectors dim 64 (functional check only).
   - Three configs, same index, same 1000 seeded queries (K=10),
     fixed-QPS then max-QPS phases: (1) baseline
     `SPTAG_AS_VECTOR_DISTANCE=0`; (2) offload, server
     `AEROSPIKE_VECTOR_SIMD=scalar`; (3) offload, server
     `AEROSPIKE_VECTOR_SIMD=neon`.
   - Metrics per config: p50/p95/p99 end-to-end search latency, achieved
     QPS, per-node CPU (docker stats sampling), network bytes returned to
     the client (client-side accounting), top-10 overlap vs baseline.
   - `quick` prints exactly `BENCH_OK` on success.
2. `docs/benchmarks/phase-4-results.md` (SPTAG-upstream): methodology
   (hardware, node count, dataset, RF, phases), full-run tables for all
   three configs, and honest caveats (single-host docker, arm64).

## Acceptance criteria

- `bash tests/benchmark/offload/run.sh quick` prints BENCH_OK.
- Results doc committed with full-run numbers for all three configs and
  overlap-vs-baseline column.

## Boundaries

- MAY TOUCH (SPTAG-upstream only): `tests/benchmark/offload/` (new),
  `docs/benchmarks/` (new), `Script_AE/` helpers.
- MUST NOT TOUCH: `docs/checks/`, `AnnService/`, `Test/src/`, `Dockerfile`,
  server repo, client repo.
- Out of scope: x86/cloud runs (recorded as residual/stretch in the digest).

Check file (read-only, in SPTAG-upstream):
`docs/checks/simd-vector-search/011-sptag-benchmark.md`
Report path (in SPTAG-upstream):
`docs/jobs/simd-vector-search/011-sptag-benchmark-01.md`
Duration hint: full benchmark ~1-2h; quick ~15m. Long steady-state phases
are not stalls.

<!-- architect-run: simd-vector-search -->

## Comments
- 2026-07-14T23:55Z [orchestrator] Implemented directly. Evidence: quick BENCH_OK (parity 1.0000 both legs; 156.1 MB -> 1.1 MB net); full run: baseline 142.5 QPS/14.06 GB vs offload-scalar 304.9 QPS/13.7 MB vs offload-neon 465.1 QPS/13.7 MB; node CPU 19% -> ~41%; overlap 0.9994/0.9986 (gate 0.99). Results + methodology + caveats in docs/benchmarks/phase-4-results.md (SPTAG repo). Harness gained functional port-probe readiness after intermittent multi-minute cluster startup stalls.
- 2026-07-14T23:55Z [orchestrator] VERDICT: PASS.
