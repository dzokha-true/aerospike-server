---
issue: 2
title: "server A1: vector kernel dispatch table, env override, hot-path hoist"
state: OPEN
parent: 1
blocked-by: none
---
Repo: aerospike-server-upstream, branch factory/simd-vector-search.

## What to build

Structural slice: introduce a runtime kernel table for tail-vector distance;
behavior must remain byte-identical to today's scalar path.

1. New `as/include/vector/vector_kernel.h` + `as/src/vector/vector_kernel.c`:
   - `typedef float (*as_vector_kernel_fn)(const void* a, const void* b,
     uint32_t dim);`
   - Kernel table indexed by (value type: float/uint8/int8/int16) x (metric
     family: l2 / cosine-family). Cosine and inner-product share one family
     (existing `compute_typed` behavior, `as/src/vector/vector_distance.cc:21-31`).
   - ISA enum: SCALAR now; SSE/AVX2/AVX512/NEON slots reserved (registration
     hooks that later slices fill; unavailable ISA slots are empty and MUST
     cause selection failure, not fallback).
   - Selection: env var `AEROSPIKE_VECTOR_SIMD` = `auto|scalar|sse|avx2|
     avx512|neon`, default `auto` (best registered). Parse/select is a pure
     function testable without process death; the server startup wrapper
     treats invalid value or unregistered ISA as fatal (loud failure, no
     silent fallback — spec requirement). One info log line names the chosen
     ISA on first resolution.
2. Register the existing scalar template instantiations
   (`sptag::DistanceUtils` wrappers in `vector_distance.cc`) as the SCALAR
   row. `as_vector_distance_compute()` keeps its exact public signature and
   semantics, now delegating through the table.
3. Hoist: `as_vector_batch_handle()` (`as/src/vector/vector_batch.c:281-303`)
   currently resolves type/metric per posting element; resolve the kernel
   once before the scan loop and call the function pointer per element.
4. New gtest `as/src/vector/vector_kernel_test.cc`: selection logic (auto ->
   scalar today; explicit scalar; invalid value -> error status; unregistered
   ISA -> error status). Register new sources in `as/src/Makefile`
   (VECTOR_SOURCES and the test source list).

## Acceptance criteria

- All 24 pre-existing vector gtests still pass; new selection tests pass.
- No wire, config, or response change; scalar results bit-identical.
- No silent fallback anywhere in selection.

## Boundaries

- MAY TOUCH: `as/include/vector/`, `as/src/vector/`, `as/src/Makefile`.
- MUST NOT TOUCH: `docs/checks/`, `docs/issues/`, `docs/runs/`, wire/protocol
  files outside as/src/vector (e.g. `as/src/base/`), `cf/`, `make_in/`.
- Out of scope: any SIMD implementation, namespace-config changes.

Check file (read-only): `docs/checks/simd-vector-search/002-server-a1-kernel-dispatch.md`
Report path: `docs/jobs/simd-vector-search/002-server-a1-kernel-dispatch-01.md`
Duration hint: build+tests ~3m on this host.

<!-- architect-run: simd-vector-search -->

## Comments
