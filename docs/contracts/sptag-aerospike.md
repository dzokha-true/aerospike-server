# SPTAG + Aerospike Contract

Standalone protocol contract between Microsoft SPTAG `AEROSPIKEIO` and this Aerospike server fork. Aerospike server code must not import SPTAG headers; all primitives below are explicit.

## Ownership

| Item | Owner | Notes |
|------|-------|-------|
| Head ANN graph | SPTAG | Includes `graph.bin`, RNG adjacency, BK-tree/KD-tree state, and graph traversal. |
| Head ID selection | SPTAG | Graph search produces specific Head IDs to fetch. |
| Posting storage | Aerospike | One record per Head ID, posting blob in a bytes bin. |
| Posting byte semantics | This contract | Aerospike decodes only for `VECTOR_DISTANCE`. |
| Tail distance today | SPTAG | `ComputeDistance` on client after MultiGet. |
| Tail distance Phase 3 | Aerospike | `VECTOR_DISTANCE` on listed local Head IDs. |
| Global top-K merge | SPTAG | Aerospike returns **Owner-Local Top K** per owner request. |

## Primitives (Phase 3)

| Name | Width | Notes |
|------|-------|-------|
| `head_id_key` | `int64_le` | Aerospike integer record key; valid `0..INT32_MAX` for SPTAG. |
| `vid` | `int32_le` | SPTAG vector ID inside posting element; valid `0..INT32_MAX`. |
| `version` | `uint8` | SPTAG posting element version; echoed, not interpreted. |
| `dimension` | config | Positive integer; namespace `vector-dimension`. |
| `value_type` | config | `float`, `uint8`, `int8`, `int16`. |
| `metric` | config | `l2`, `cosine`, `inner-product`. |
| `distance` | `float32_le` | SPTAG-compatible score; smaller is better. |

Endianness: all multi-byte fields in EC528 payloads are **little-endian**. Big-endian hosts are unsupported in Phase 3.

## Posting layout

One Posting Element:

```text
[vid:int32_le][version:uint8][vector_payload: dimension * sizeof(value_type)]
```

Posting stride:

```text
5 + dimension * value_type_size
```

Posting blob: concatenated Posting Elements only; **no** count prefix, **no** header.

Malformed if `blob_size % stride != 0`, or any element has `vid < 0`.

Raw KV blobs only. Compressed/rearrange/delta static layouts are out of scope.

## Namespace config

Mandatory (must match SPTAG index config):

```text
vector-dimension <n>
vector-value-type float|uint8|int8|int16
vector-metric l2|cosine|inner-product
```

Optional limits:

```text
vector-max-head-ids <n>
vector-max-topk <n>
vector-max-query-bytes <n>
vector-max-response-bytes <n>
```

Query vector length must equal `dimension * value_type_size`, and must be `<= vector-max-query-bytes` when that limit is set.

## Distance semantics

Mirror SPTAG `ComputeDistance` / posting-scan path:

- `l2`: sum of squared differences (float accumulator).
- `cosine` and `inner-product`: `base * base - dot` where `base` is `1` for `float`, else `numeric_max(T)` per SPTAG `Utils::GetBase`.
- No server normalization. SPTAG must send query bytes already compatible with postings.
- No quantizer/PQ/OPQ unless a future contract adds metadata.

## VECTOR_DISTANCE wire path

- Message: `AS_MSG_INFO1_BATCH` set.
- Fields: standard `namespace` field + `AS_MSG_FIELD_TYPE_VECTOR_DISTANCE` (44).
- **Not** `AS_MSG_FIELD_TYPE_BATCH` (41) for this op.

### Request payload v1

```text
uint8   version = 1
uint8   reserved = 0
uint16  reserved = 0
uint32  topk                    // 1..vector-max-topk
uint16  bin_name_len
uint16  set_name_len            // 0 = no set
uint16  query_bytes_len
uint32  head_id_count           // > 0; duplicate head_id_key deduped
uint8   bin_name[bin_name_len]
uint8   set_name[set_name_len]
uint8   query_bytes[query_bytes_len]
int64   head_id_key[head_id_count]   // int64_le each
```

Request-level `bad-request` if: unsupported version, `topk == 0`, empty head list, out-of-range head_id_key, query length mismatch, limits exceeded, duplicate head IDs after dedupe still invalid, or estimated response size `> vector-max-response-bytes`.

### Response payload v1

Returned in field `AS_MSG_FIELD_TYPE_VECTOR_DISTANCE_RESPONSE` (45).

```text
uint8   version = 1
uint8   request_status          // see status table
uint16  reserved = 0
uint32  result_count
uint32  key_status_count
// results[result_count]:
//   int64  head_id_key
//   int32  vid
//   uint8  version
//   uint8  reserved
//   float32 distance
// key_statuses[key_status_count]:
//   int64  head_id_key
//   uint8  key_status
//   uint8  reserved[3]
```

**Owner-Local Top K**: select `topk` best results across all successfully scored tails in this request. Dedupe by `vid` within request (keep best by distance, then vid, head_id_key, version ascending).

### Status values

Request-level `request_status`:

| Value | Name |
|-------|------|
| 0 | `ok` |
| 1 | `bad-request` |
| 2 | `bad-config` |
| 3 | `unsupported-version` |
| 4 | `limit-exceeded` |
| 5 | `response-too-large` |
| 6 | `server-error` |

Per-key `key_status`:

| Value | Name |
|-------|------|
| 0 | `ok` |
| 1 | `wrong-owner` |
| 2 | `not-found` |
| 3 | `bin-not-found` |
| 4 | `bad-bin-type` |
| 5 | `malformed-posting` |

Accept `BLOB` and `VECTOR` particle types as raw byte buffers.

## SPTAG integration assumptions

- SPTAG supplies Head IDs after graph traversal; Aerospike does not discover heads.
- SPTAG stores records keyed by integer Head ID (`as_key_init_int64` in SPTAG client).
- SPTAG owns global merge across owner-node responses.
- SPTAG owns stale/deleted VID validation unless future protocol adds expected versions.
- Default posting bin name in SPTAG is `value`; request may override via `bin_name`.

## Out of contract

- Graph storage or traversal on server.
- Partition scan or brute-force namespace scan.
- Lua UDF hot path.
- Server forwarding for wrong-owner keys.
- Quantized index paths without explicit contract extension.
