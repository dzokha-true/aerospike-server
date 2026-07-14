# ADR 0004: VECTOR_DISTANCE Protocol and Semantics

## Status

Accepted

## Context

Phase 3 adds server-side tail-vector distance for SPTAG `AEROSPIKEIO`. SPTAG remains the smart client: graph traversal, Head ID selection, and global merge stay in SPTAG. Aerospike must score posting blobs only for explicitly listed Head IDs on the partition owner node.

Requirements gathered during design review:

- No SPTAG typedefs in server code; explicit Aerospike primitives only.
- Mirror SPTAG `ComputeDistance` (not `AccurateDistance`).
- Support all posting `ValueType`s: `float`, `uint8`, `int8`, `int16`.
- `inner-product` uses the same function family as SPTAG cosine distance (`base * base - dot`), smaller-is-better.
- No server normalization; SPTAG supplies query bytes in the same representation as postings.
- No quantizer/PQ/OPQ in Phase 3.
- Raw Aerospike KV posting blobs only (no compression/rearrange/delta variants).

## Decision

### Wire entry point

Use `AS_MSG_INFO1_BATCH` with a new message field `AS_MSG_FIELD_TYPE_VECTOR_DISTANCE` (44). Do **not** use a normal bin op as the primary path.

The field payload is a versioned little-endian binary struct (v1). Namespace comes from the standard namespace field; set name and bin name are in the payload.

### Response

Return `Owner-Local Top K` tuples `(head_id_key:int64, vid:int32, version:uint8, distance:float32)` plus per-key statuses for keys that could not be scored. Request-level failure for invalid request/config, unsupported payload version, limit exceeded, or `response-too-large`.

### Partition behavior

- Score only records owned by the contacted node (local master read).
- Per-key `wrong-owner` for keys not local; no server forwarding.
- Partial success: valid keys score; invalid keys get per-key status.

### Distance implementation

Vendor-adapted SPTAG `DistanceUtils` scalar code under `as/src/vector/` (`sptag_distance.h`, `vector_distance.cc`) with Microsoft MIT license preserved.

**Shipped (run simd-vector-search, 2026-07):** SIMD kernels behind a runtime
kernel table (`vector_kernel.{h,c}`), selected once per process from
`AEROSPIKE_VECTOR_SIMD` (`auto|scalar|sse|avx2|avx512|neon`; invalid or
unavailable values crash the first VECTOR_DISTANCE request - no fallback;
the chosen ISA is logged once). Per-ISA translation units with per-object
compile flags (`-march=nocona` baseline untouched):

- `vector_distance_neon.cc` - aarch64 baseline (armv8 always has ASIMD).
- `vector_distance_{sse,avx2,avx512}.cc` - x86-64, runtime-gated by
  `vector_cpu.c` (CPUID leaf 1/7 plus XGETBV XCR0 ymm/zmm checks; SSE row
  needs SSE4.1, AVX-512 row needs F+BW).

`as_vector_distance_compute()` stays pinned to the scalar row as the parity
oracle; the batch handler resolves the active kernel once per request, not
per posting element. Parity: per-element math is identical; lane-parallel
float accumulation reorders additions, bounded by
`1e-5 relative + 4*base^2*dim*eps` (see `docs/benchmarks/simd-kernels.md`).
Measured NEON speedup 3.4-4.7x (float), 5.5-9.1x (int8) over scalar.

### Namespace config

Mandatory: `vector-dimension`, `vector-value-type`, `vector-metric`.

Optional limits: `vector-max-head-ids`, `vector-max-topk`, `vector-max-query-bytes`, `vector-max-response-bytes`.

## Consequences

- SPTAG forked Aerospike C client must send the new batch-style request (Phase 4).
- Contract and conformance tests are the source of truth for byte layout.
- Server does not interpret SPTAG version-map or deletion state; echoes `version` byte only.

## Alternatives Considered

### Normal read op `AS_MSG_OP_VECTOR_DISTANCE`

Rejected. `process_bin_read_op()` is single-record scoped; multi-key scoring needs batch-style payload.

### Server forwarding for wrong-owner keys

Rejected. Violates partition-local contract and duplicates smart-client fanout.

### Raw inner-product (larger is better)

Rejected. Breaks SPTAG `DistanceCalcSelector` parity and merge semantics.
