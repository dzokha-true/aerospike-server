# Check: 012 server docs finish

Executor: bash
Spec: docs/spec/aerospike-simd-vector-search.md
Issue: docs/issues/simd-vector-search/012-server-docs-finish.md

## Runnable

- RUN: `git grep -n "AEROSPIKE_VECTOR_SIMD" -- docs/adr/0004-vector-distance-protocol.md | head -2` -> ADR 0004 records the shipped SIMD mechanism; exit 0.
- RUN: `git grep -in "neon" -- docs/adr/0004-vector-distance-protocol.md | head -2` -> NEON policy recorded; exit 0.
- RUN: `ls docs/solutions | head -10` -> at least one solutions entry from this run; exit 0.
- RUN: `git grep -in "complete\|shipped\|done" -- docs/agent-phases/phase-4-sptag-integration.md | head -3` -> phase-4 status updated; exit 0.

## Judge-only (orchestrator-graded; no cold judge for the finish docs job)

- Docs statements match run artifacts (benchmarks, smoke, parity evidence);
  no aspirational claims.
