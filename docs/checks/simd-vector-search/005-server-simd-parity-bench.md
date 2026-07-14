# Check: 005 server SIMD parity suite + microbench

Executor: bash
Spec: docs/spec/aerospike-simd-vector-search.md
Issue: docs/issues/simd-vector-search/005-server-simd-parity-bench.md
Duration hint: native ~5m; emulated x86 leg ~10-25m; not a stall.

## Runnable

- RUN: `make -C as run-vector-tests > .architect/tmp/vt.log 2>&1; s=$?; tail -4 .architect/tmp/vt.log; exit $s` -> exit 0; suite green including parity tests.
- RUN: `./target/$(uname -s)-$(uname -m)/bin/vector_unit_tests --gtest_filter='*Parity*' 2>&1 | tail -4` -> parity tests ran and passed natively (0 matched = FAIL).
- RUN: `make -C as vector-bench > .architect/tmp/vb.log 2>&1 && ./target/$(uname -s)-$(uname -m)/bin/vector_bench 2>&1 | head -20` -> bench builds and emits a markdown table with ns/vector rows for scalar and neon; exit 0.
- RUN: `grep -ci "tolerance" docs/benchmarks/simd-kernels.md` -> count >= 1 (tolerance policy documented); exit 0.
- RUN: `grep -ci "emulated\|rosetta" docs/benchmarks/simd-kernels.md` -> count >= 1 (x86 numbers labeled emulated); exit 0.
- RUN: `bash tools/vector-x86-test.sh > .architect/tmp/x86.log 2>&1; s=$?; tail -6 .architect/tmp/x86.log; exit $s` -> exit 0, contains `X86_TESTS_OK` with parity tests included.

## Judge-only

- Parity test logs its seed; dims cover 1..67 plus {100,128,768,1024}.
- Results doc has both tables (native arm64; emulated x86) and does not
  present emulated numbers as performance claims.
- Any production-kernel fix made by this job is explicitly recorded in the
  job report.
