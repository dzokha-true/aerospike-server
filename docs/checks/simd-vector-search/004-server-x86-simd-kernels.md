# Check: 004 server x86 SIMD kernels + CPUID

Executor: bash
Spec: docs/spec/aerospike-simd-vector-search.md
Issue: docs/issues/simd-vector-search/004-server-x86-simd-kernels.md
Duration hint: emulated x86 leg ~10-25m; not a stall.

## Runnable

- RUN: `make -C as run-vector-tests > .architect/tmp/vt.log 2>&1; s=$?; tail -4 .architect/tmp/vt.log; exit $s` -> exit 0 on this arm64 host (x86 TUs arch-excluded, suite still green).
- RUN: `bash tools/vector-x86-test.sh > .architect/tmp/x86.log 2>&1; s=$?; tail -8 .architect/tmp/x86.log; exit $s` -> exit 0 and output contains `X86_TESTS_OK`; SSE and AVX2 parity tests ran (not skipped); AVX-512 tests skipped under Rosetta.
- RUN: `grep -l "MIT" as/src/vector/vector_distance_sse.cc as/src/vector/vector_distance_avx2.cc as/src/vector/vector_distance_avx512.cc` -> all three files listed (license headers preserved); exit 0.
- RUN: `git grep -in "xgetbv\|xcr0" -- as/src/vector/vector_cpu.c | head -3` -> OS-enabled-state check present; exit 0.
- RUN: `git grep -n "mavx2\|mavx512" -- as/src/Makefile | head -4` -> per-object ISA flags in the Makefile; exit 0.

## Judge-only

- ISA flags are attached per object only; `-march=nocona` baseline and global
  CFLAGS unchanged.
- Vendored TUs contain no SPTAG typedefs (`DimensionType`, `DistCalcMethod`).
- CPUID logic: AVX requires OSXSAVE + XCR0 ymm; AVX-512 additionally requires
  XCR0 zmm/opmask and leaf-7 AVX512F (+BW where int kernels need it) — cite
  file:line for each.
- The x86 log shows runtime ISA detection output consistent with Rosetta
  (AVX2 yes, AVX-512 no).
