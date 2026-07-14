---
issue: 5
title: "server A4: randomized SIMD parity suite + kernel microbenchmark"
state: OPEN
parent: 1
blocked-by: 4
---
Repo: aerospike-server-upstream, branch factory/simd-vector-search.

## What to build

1. `as/src/vector/vector_parity_test.cc`: comprehensive randomized parity
   gtest — logged seed, every dim in 1..67 plus {100,128,768,1024}, all four
   value types x both metric families x every runtime-available ISA vs the
   scalar oracle. Integer-input kernels: exact match required unless the
   vendored kernel provably reorders float accumulation (then relative
   1e-5, documented in the test and in the results doc). Float kernels:
   relative tolerance 1e-5.
2. `as/src/vector/vector_bench.cc` -> `target/.../bin/vector_bench`
   (new Makefile target `vector-bench`, built like the test binary but
   without gtest): ns/vector for dims {128, 768, 1024}, types {float, int8},
   each registered ISA, enough iterations for stable numbers; markdown table
   to stdout.
3. `docs/benchmarks/simd-kernels.md` (new): tolerance policy, methodology,
   and measured tables for this arm64 host (scalar vs NEON, native) and the
   emulated x86 run (scalar vs SSE vs AVX2) clearly labeled EMULATED
   (Rosetta) — informational only, not a performance claim.

## Acceptance criteria

- Parity suite green natively (scalar+NEON) and under
  `tools/vector-x86-test.sh` (scalar+SSE+AVX2).
- `vector_bench` builds and runs; results doc committed with both tables and
  the tolerance policy.

## Boundaries

- MAY TOUCH: `as/src/vector/`, `as/include/vector/`, `as/src/Makefile`,
  `docs/benchmarks/simd-kernels.md`, `tools/vector-x86-test.sh` (only if the
  bench needs a hook; keep the `X86_TESTS_OK` contract).
- MUST NOT TOUCH: `docs/checks/`, `docs/issues/`, `docs/runs/`, production
  kernel code from #2-#4 except bug fixes required by a failing parity test
  (record any such fix explicitly in the report).
- Out of scope: end-to-end/cluster benchmarks (issue #11).

Check file (read-only): `docs/checks/simd-vector-search/005-server-simd-parity-bench.md`
Report path: `docs/jobs/simd-vector-search/005-server-simd-parity-bench-01.md`
Duration hint: native ~5m; emulated x86 pass ~10-20m.

<!-- architect-run: simd-vector-search -->

## Comments
