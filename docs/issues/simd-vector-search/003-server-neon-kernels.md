---
issue: 3
title: "server A3: NEON distance kernels (arm64)"
state: CLOSED
parent: 1
blocked-by: 2
---
Repo: aerospike-server-upstream, branch factory/simd-vector-search.

## What to build

1. `as/src/vector/vector_distance_neon.cc`: NEON (`arm_neon.h`) kernels for
   all four value types (float32, uint8, int8, int16) x both metric families
   (l2, cosine-family). Semantics mirror the scalar kernels in
   `as/include/vector/sptag_distance.h` exactly: float accumulation,
   `GetBase()` handling for the cosine family (`base*base - dot`,
   smaller-is-better), identical remainder-element handling at the tail of
   the loop. Guard the whole TU with `#if defined(__aarch64__)`.
2. Register the NEON row in the kernel table from issue #2. On aarch64,
   `auto` selects NEON; `AEROSPIKE_VECTOR_SIMD=scalar` still forces scalar;
   `neon` on non-aarch64 builds is a selection error (no fallback).
3. `as/src/Makefile`: add the TU (aarch64-only via the existing `ARCH`
   conditional pattern, see `make_in/Makefile.in:74-88` for the arch split);
   the arm64 baseline `-mcpu=neoverse-n1` already includes NEON — no extra
   ISA flags needed, but do not widen global flags.
4. Gtest `as/src/vector/vector_neon_test.cc`: parity vs scalar for dims
   {1,3,4,7,8,15,16,31,32,33,64,100,128,768} on seeded random data, all
   types x metrics. Integer inputs: exact float equality expected (same
   float accumulation order must be preserved where feasible; if NEON lane
   reduction changes float summation order, document and use relative
   tolerance 1e-5 for float-typed inputs — integers converted through float
   may also need it; state which in the test). `GTEST_SKIP()` on
   non-aarch64.

## Acceptance criteria

- Full vector gtest suite green on this Darwin arm64 host, including NEON
  parity tests actually running (not skipped).
- `auto` on arm64 logs NEON as the chosen ISA.

## Boundaries

- MAY TOUCH: `as/include/vector/`, `as/src/vector/`, `as/src/Makefile`.
- MUST NOT TOUCH: `docs/checks/`, `docs/issues/`, `docs/runs/`, `cf/`,
  `make_in/`, anything outside the vector module.
- Out of scope: x86 kernels, benchmarks.

Check file (read-only): `docs/checks/simd-vector-search/003-server-neon-kernels.md`
Report path: `docs/jobs/simd-vector-search/003-server-neon-kernels-01.md`
Duration hint: build+tests ~3m.

<!-- architect-run: simd-vector-search -->

## Comments
- 2026-07-14T06:20Z [orchestrator] Implemented directly (owner directive). Evidence: 36/36 gtests (was 31); VectorNeon suite ran 5 tests on this arm64 host including 4 parity cases (all types x l2/cosine/inner-product, dims 1..768, rel 1e-5); vld1q intrinsics present; __aarch64__ guard present; auto->neon verified by test. Commit 7f0c015.
- 2026-07-14T06:20Z [orchestrator] VERDICT: PASS - frozen RUN items green; float-lane accumulation tolerance documented in test header.
