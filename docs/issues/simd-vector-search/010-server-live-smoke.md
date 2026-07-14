---
issue: 10
title: "server C1: live VECTOR_DISTANCE smoke, scalar vs NEON wire equivalence"
state: OPEN
parent: 1
blocked-by: 5, 8
---
Repo: aerospike-server-upstream, branch factory/simd-vector-search.

## What to build

The live conformance harness the Phase-3 README defers
(`tests/conformance/vector_distance/README.md` marks `smoke.py` as planned):

1. `tests/conformance/vector_distance/smoke.py`, pure stdlib Python: uses the
   existing `codec.py` to speak the wire protocol over a raw socket to a
   live asd container (image from issue #8; the script accepts
   `SERVER_IMAGE`/host/port env, and a `--no-docker` mode that targets an
   already-running server). Extend `codec.py` minimally with a standard
   single-bin write message so smoke can write posting-blob fixtures itself
   (small: 4 head keys, ~50 tail vectors each, dim 64 float32 L2; seeded).
2. Cases: (a) scored results match a local Python reference distance within
   1e-5 relative; (b) per-key statuses for a missing key and a malformed
   posting; (c) top-K truncation; (d) run the whole set twice against
   containers started with `AEROSPIKE_VECTOR_SIMD=scalar` and `=neon` —
   identical statuses, distances within 1e-5 relative, same result sets.
3. `tests/conformance/vector_distance/run-smoke.sh`: brings up the
   container(s), runs smoke.py for both SIMD modes, tears down, prints
   exactly `SMOKE_OK` on success.
4. Update the conformance README (smoke is no longer "planned").

## Acceptance criteria

- `bash tests/conformance/vector_distance/run-smoke.sh` prints SMOKE_OK.
- Offline codec tests still pass:
  `python3 -m unittest tests.conformance.vector_distance.test_codec`.

## Boundaries

- MAY TOUCH: `tests/conformance/vector_distance/` only.
- MUST NOT TOUCH: `docs/checks/`, `docs/issues/`, `as/`, `docker/` (consume
  the image; if it lacks something, report BLOCKED with evidence).
- Out of scope: benchmarks, x86 modes.

Check file (read-only): `docs/checks/simd-vector-search/010-server-live-smoke.md`
Report path: `docs/jobs/simd-vector-search/010-server-live-smoke-01.md`
Duration hint: ~10m plus one image build if not cached.

<!-- architect-run: simd-vector-search -->

## Comments
