# Phase 3 VECTOR_DISTANCE conformance

Two layers cover SPEC-3 today:

| Layer | Path | Status |
|-------|------|--------|
| Wire + digest layout (Python, no server) | `tests/conformance/vector_distance/test_codec.py` | runs in CI |
| Live single-node smoke (asd + raw socket) | `tests/conformance/vector_distance/smoke.py` | manual, requires Linux build |

## What `test_codec.py` locks

- Request header is 18 bytes; `head_count` is `uint32_le` at offset 14.
- Response result element is 18 bytes; key-status element is 12 bytes; the
  size formula matches `as_vector_wire_encode_response_size`.
- Digest derivation for an `int64` head_id_key matches the canonical
  Aerospike client (`set_name + 0x01 + be64(key)`, RIPEMD-160). The
  namespace must NOT be in the hash; the server's
  `as_vector_digest_compute` is verified against this Python reference.

Drift in either the encoder/decoder or the digest function breaks these
tests, so the SPTAG client and server cannot silently disagree.

## What `smoke.py` is meant to do (TODO Phase 4)

1. Start asd with a vector-enabled namespace.
2. Insert a record at `head_id_key=42` whose blob bin holds a synthetic
   posting list with two elements (vid, version, payload).
3. Open a raw TCP socket, send a `VECTOR_DISTANCE` batch request via
   `codec.build_msg(...)`, and decode the response.
4. Assert top-K ordering, dedupe, and per-key status code paths.

The harness depends on the SPTAG-side write path (Phase 4) for record
seeding. Until that lands, run the codec tests in CI and the live smoke
manually with a hand-seeded record:

```bash
python3 -m unittest tests.conformance.vector_distance.test_codec
```
