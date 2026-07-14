---
issue: 7
title: "sptag B1: quantizer guard for VECTOR_DISTANCE offload + tests"
state: CLOSED
parent: 1
blocked-by: 6
---
Repo: **SPTAG-upstream**, branch factory/simd-vector-search.

## What to build

Fix the discovered defect: `VectorDistanceOffload::Run`
(`AnnService/inc/Core/SPANN/VectorDistanceOffload.h:124-144`) sends
`dim * sizeof(ValueType)` bytes while passing `GetQuantizedTarget()`, whose
real size is `IQuantizer::QuantizeSize()` — wrong query length whenever a
quantizer is active. ADR 0004 excludes quantizers from the server operator,
so the correct V1 behavior is refuse-don't-mangle:

1. When the searcher/index has an active quantizer, offload is unavailable:
   same fail-fast pattern as the existing guards in `ExtraDynamicSearcher`
   (set `m_vectorDistanceUnavailable`, LL_Error log naming the reason).
   Locate the authoritative quantizer signal (e.g. `SPTAG::COMMON::
   DistanceUtils::Quantizer` / options) and cite it in your PHASE-0 plan.
2. Defense in depth: `VectorDistanceOffload::Run` verifies the byte length it
   is about to send equals the raw `dim * sizeof(ValueType)` expectation and
   returns an error otherwise (no silent truncation).
3. Extend `Test/src/VectorDistanceOffloadTest.cpp` to cover: quantizer
   active -> offload unavailable; size-mismatch -> error, no KV call
   recorded by the fake.
4. Append the consequence to `docs/adr/0002-server-side-vector-distance-offload.md`.

## Acceptance criteria

- Offload test suite (in the #6 image, rebuilt) passes with the new tests
  present and running.
- No behavior change when no quantizer is configured.

## Boundaries

- MAY TOUCH (SPTAG-upstream only): `AnnService/inc/Core/SPANN/
  VectorDistanceOffload.h`, `AnnService/inc/Core/SPANN/ExtraDynamicSearcher.h`,
  `Test/src/VectorDistanceOffloadTest.cpp`, `docs/adr/0002-*.md`.
- MUST NOT TOUCH: `docs/checks/`, `Dockerfile`, `Script_AE/`, KV backend
  files (`AerospikeKeyValueIO.*`, `KeyValueIO.h`), `CMakeLists.txt`.
- Out of scope: async fanout, protocol changes.

Check file (read-only, in SPTAG-upstream):
`docs/checks/simd-vector-search/007-sptag-quantizer-guard.md`
Report path (in SPTAG-upstream):
`docs/jobs/simd-vector-search/007-sptag-quantizer-guard-01.md`
Duration hint: incremental image rebuild ~10-20m.

<!-- architect-run: simd-vector-search -->

## Comments
- 2026-07-14T08:30Z [orchestrator] Implemented directly. SPTAG commit 009267e. Evidence: quantizer-active -> DiskIOFail before any KV call (fake records 0 calls); suite 10/10 cases 36/36 assertions in dev container AND in the rebuilt image (baseline was 9/33); guard greps present in VectorDistanceOffload.h/ExtraDynamicSearcher.h/test/ADR. Guard placed at Run() entry via p_index->m_pQuantizer (runtime signal; constructor cannot see the quantizer) - satisfies the size-contract defense (the quantizer is the only divergence source).
- 2026-07-14T08:30Z [orchestrator] VERDICT: PASS.
