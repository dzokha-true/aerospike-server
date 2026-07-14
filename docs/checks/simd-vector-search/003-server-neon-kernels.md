# Check: 003 server NEON kernels

Executor: bash
Spec: docs/spec/aerospike-simd-vector-search.md
Issue: docs/issues/simd-vector-search/003-server-neon-kernels.md
Duration hint: full build+test ~3m.

## Runnable

- RUN: `make -C as run-vector-tests > .architect/tmp/vt.log 2>&1; s=$?; tail -4 .architect/tmp/vt.log; exit $s` -> exit 0; all tests PASSED, total strictly greater than the issue-002 count.
- RUN: `./target/$(uname -s)-$(uname -m)/bin/vector_unit_tests --gtest_filter='*Neon*' 2>&1 | tail -4` -> at least one Neon-named test RAN and PASSED on this arm64 host (0 tests matched = FAIL; skipped = FAIL).
- RUN: `git grep -c "vld1q" -- as/src/vector/vector_distance_neon.cc` -> count >= 1 (real NEON load intrinsics); exit 0.
- RUN: `git grep -n "__aarch64__" -- as/src/vector/vector_distance_neon.cc | head -2` -> TU arch guard present; exit 0.

## Judge-only

- Parity tests cover all four value types x both metric families across the
  dim list from the issue; seeded random data; tolerance rule stated in the
  test (integers exact unless documented; float relative 1e-5).
- NEON is registered so `auto` selects it on aarch64; explicit `neon` on
  non-aarch64 is an error, not a fallback (cite file:line).
- Global compile flags unchanged (`make_in/Makefile.in` untouched).
