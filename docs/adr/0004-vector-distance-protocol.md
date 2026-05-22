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

Vendor adapted SPTAG `DistanceUtils` / `InstructionUtils` under `as/src/vector/` with Microsoft MIT license preserved. Scalar fallback always built; SIMD in separate translation units with runtime CPUID dispatch on x86.

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
