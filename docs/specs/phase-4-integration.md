---
id: SPEC-4-PHASE
phase: 4
status: draft
contract: docs/contracts/sptag-aerospike.md
tests: []
---

# Phase 4 SPTAG integration

Behavior specs for wiring SPTAG to server-side `VECTOR_DISTANCE`. Implementation guide: [`../agent-phases/phase-4-sptag-integration.md`](../agent-phases/phase-4-sptag-integration.md).

Server wire format is field types **44/45** with `AS_MSG_INFO1_BATCH` (see ADR 0004), not `AS_MSG_OP_*`.

## SPEC-4-INTEG-001: Top-K parity vs ComputeDistance baseline

### Given

Same index, query, and head ID set; feature flag off uses SPTAG `ComputeDistance` on MultiGet postings; flag on uses parallel `VECTOR_DISTANCE` per owner node.

### When

Search completes with both paths.

### Then

Global top-K overlaps baseline above an agreed threshold (tie ordering may differ); no systematic distance inversion or missing heads that baseline scored.

### Error cases

Metric, dimension, or value-type mismatch between SPTAG index and Aerospike namespace config → fail fast with documented error.

## SPEC-4-BENCH-001: Latency and QPS methodology

### Given

Documented hardware, dataset, dimension, node count, RF, and heartbeat mode (mesh for &gt;8 nodes).

### When

Benchmark runs baseline vs `VECTOR_DISTANCE` at matched QPS.

### Then

Record p99 end-to-end search latency, sustained QPS, SPTAG CPU, and per-node Aerospike CPU in `docs/benchmarks/phase-4-results.md`.

### Error cases

Benchmark without passing SPEC-4-INTEG-001 does not satisfy this spec.
