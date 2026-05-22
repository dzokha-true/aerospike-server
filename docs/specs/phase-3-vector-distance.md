---
id: SPEC-3-PHASE
phase: 3
status: verified
contract: docs/contracts/sptag-aerospike.md
tests:
  - as/src/vector/vector_math_test.cc
  - as/src/vector/vector_posting_test.cc
  - as/src/vector/vector_topk_test.cc
  - as/src/vector/vector_wire_test.cc
  - as/src/vector/vector_cfg_test.cc
  - tests/conformance/vector_distance/test_codec.py
---

# Phase 3 Vector Distance

## SPEC-3-CFG-001: Valid namespace vector config

### Given

Namespace stanza with `vector-dimension`, `vector-value-type`, `vector-metric`.

### When

Server parses config.

### Then

Values stored on namespace; `query_bytes = dimension * value_type_size <= vector-max-query-bytes` when limit set.

### Error cases

Invalid value type, metric, or dimension rejected at config load.

## SPEC-3-PARSER-001: Parse posting elements

### Given

Blob of repeated `[vid:int32_le][version:uint8][payload]` matching dimension and value type.

### When

Parser walks blob.

### Then

Returns elements with correct vid, version, payload pointer.

### Error cases

`blob_size % stride != 0` → malformed. `vid < 0` → malformed.

## SPEC-3-VDIST-001: SPTAG distance parity

### Given

Known query/tail pairs for `l2`, `cosine`, `inner-product` and all value types.

### When

`as_vector_distance_compute` runs.

### Then

Matches SPTAG `ComputeDistance` reference outputs (smaller-is-better).

### Error cases

Length mismatch returns error.

## SPEC-3-TOPK-001: Owner-local top K

### Given

Multiple heads with multiple tails; `topk` smaller than total tails.

### When

Accumulator runs.

### Then

Returns K best by (distance, vid, head_id_key, version); dedupes by vid.

## SPEC-3-WIRE-001: Request/response codec

### Given

v1 request/response structs.

### When

Encode then decode.

### Then

Round-trip preserves fields; little-endian.

### Error cases

Unsupported version, wrong query length, `topk == 0` rejected.

## SPEC-3-OP-001: Wrong owner per-key status

### Given

Head ID owned by another node.

### When

`VECTOR_DISTANCE` runs locally.

### Then

Per-key `wrong-owner`; other local keys still score.

## SPEC-3-OP-002: Malformed posting per-key

### Given

One head with invalid blob length.

### When

Request lists that head and a valid head.

### Then

Invalid head `malformed-posting`; valid head scores.
