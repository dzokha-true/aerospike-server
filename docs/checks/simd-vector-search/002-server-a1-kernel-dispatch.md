# Check: 002 server A1 kernel dispatch table

Executor: bash
Spec: docs/spec/aerospike-simd-vector-search.md
Issue: docs/issues/simd-vector-search/002-server-a1-kernel-dispatch.md
Duration hint: full build+test ~3m.

## Runnable

- RUN: `make -C as run-vector-tests > .architect/tmp/vt.log 2>&1; s=$?; tail -4 .architect/tmp/vt.log; exit $s` -> exit 0; last lines report all tests PASSED with a total strictly greater than 24 (the 24 pre-existing tests plus new selection tests).
- RUN: `git grep -l "AEROSPIKE_VECTOR_SIMD" -- as/src/vector as/include/vector | head -3` -> at least one source path printed (env override implemented); exit 0.
- RUN: `git grep -n "as_vector_kernel" -- as/src/vector/vector_batch.c | head -5` -> kernel-table symbol used in the batch scan path; exit 0.
- RUN: `git grep -n "as_vector_distance_compute" -- as/include/vector/vector_distance.h | head -3` -> public compute API still declared (signature preserved); exit 0.

## Judge-only

- In the diff, `as_vector_batch_handle()` resolves the kernel once before the
  posting scan loop; no per-element type/metric switch remains on the hot path.
- Selection has no silent fallback: invalid env value or explicitly requested
  unregistered ISA is a loud error path (cite file:line).
- No changes outside MAY-TOUCH; no wire/config/response changes.
