# Check: 010 server live smoke (scalar vs NEON wire equivalence)

Executor: bash
Spec: docs/spec/aerospike-simd-vector-search.md
Issue: docs/issues/simd-vector-search/010-server-live-smoke.md
Duration hint: ~10m with cached image.

## Runnable

- RUN: `python3 -m unittest tests.conformance.vector_distance.test_codec 2>&1 | tail -3` -> OK, offline codec suite unbroken; exit 0.
- RUN: `bash tests/conformance/vector_distance/run-smoke.sh > .architect/tmp/smoke.log 2>&1; s=$?; tail -6 .architect/tmp/smoke.log; exit $s` -> exit 0 and output contains `SMOKE_OK`.
- RUN: `grep -c "neon\|scalar" tests/conformance/vector_distance/run-smoke.sh` -> count >= 2 (both SIMD modes exercised); exit 0.
- RUN: `git grep -n "smoke" -- tests/conformance/vector_distance/README.md | head -3` -> README updated, smoke no longer only "planned"; exit 0.

## Judge-only

- smoke.py writes its own posting fixtures over the wire (no external client
  dependency) and validates distances against a local Python reference within
  1e-5 relative; per-key status and top-K truncation cases present.
- The scalar and neon legs compare statuses and result sets, distances within
  1e-5 relative (cite the comparison code file:line).
- Diff confined to `tests/conformance/vector_distance/`.
