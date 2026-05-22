# ADR 0002: SPTAG Posting Blob Format

## Status

Accepted

## Context

Aerospike `AEROSPIKEIO` records store SPTAG posting lists as opaque value blobs. Phase 3 server-side distance decodes those bytes using namespace config that must match SPTAG index config.

SPTAG `ExtraDynamicSearcher` stores each posting list as concatenated vector-info units (raw KV path, no compression).

## Decision

Keep the SPTAG posting blob layout unchanged.

One Aerospike record is keyed by Head ID (`int64` user key; SPTAG uses `SizeType` in `0..INT32_MAX`) and contains one Posting List blob. The blob contains concatenated Posting Elements:

```text
[int32_le vid][uint8 version][dimension * sizeof(value_type) payload]
```

Stride:

```text
5 + dimension * sizeof(value_type)
```

`value_type` config strings: `float` (4), `uint8` (1), `int8` (1), `int16` (2).

Valid `vid` range: `0..INT32_MAX`. Negative `vid` makes the whole posting **malformed**.

All multi-byte fields are **little-endian** on the wire and in blobs for Phase 3.

Phase 3 namespace config must include:

- `vector-dimension`
- `vector-value-type`
- `vector-metric`

Distance metric is not encoded in posting bytes.

## Consequences

- Existing SPTAG `AEROSPIKEIO` writes remain compatible.
- Server validates `blob_size % stride == 0` before reading elements.
- Tests use hex fixtures with explicit primitive widths.

## Alternatives Considered

### Repack posting lists

Rejected. Breaks SPTAG write/read compatibility.

### Float-only assumption

Rejected. SPTAG supports Int8/UInt8/Int16 index value types.
