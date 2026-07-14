---
issue: 8
title: "server: Linux asd image + vector-configured single/3-node compose"
state: OPEN
parent: 1
blocked-by: 2
---
Repo: aerospike-server-upstream, branch factory/simd-vector-search.

## What to build

1. `docker/Dockerfile`: multi-stage Ubuntu 22.04 build of `asd` from THIS
   source tree (submodules are initialized in the repo — COPY them; no
   network fetch of source). Native arch (arm64 on this host). Runtime stage
   with a thin entrypoint.
2. `docker/aerospike-vector.conf`: CE config with a `test` namespace carrying
   the mandatory vector config (`vector-dimension`, `vector-value-type`,
   `vector-metric` — see `as/src/vector/vector_namespace_server.c` and
   `docs/specs/phase-3-vector-distance.md` for exact stanza names). Make
   dimension/value-type/metric settable via env substitution in the
   entrypoint (defaults: 64 / float / l2) so integration and benchmark
   slices can reuse the image. `AEROSPIKE_VECTOR_SIMD` env must pass through
   to asd's environment.
3. `docker/compose-single.yml` and `docker/compose-cluster3.yml` (3 nodes,
   mesh heartbeat, one exposed client port per node).
4. `docker/smoke-up.sh`: brings up the single node, waits for cluster-ready,
   verifies the vector namespace config is live (e.g. `asinfo` /
   `docker exec` grep of the log line the vector config prints), tears down,
   prints exactly `SERVER_VECTOR_READY` on success, nonzero exit otherwise.
5. `docker/README.md`: build/run commands.

## Acceptance criteria

- `docker build -f docker/Dockerfile .` succeeds on this arm64 host.
- `bash docker/smoke-up.sh` prints SERVER_VECTOR_READY.
- No source-tree changes outside `docker/`.

## Boundaries

- MAY TOUCH: `docker/` (new directory) only.
- MUST NOT TOUCH: `docs/checks/`, `docs/issues/`, `as/`, `cf/`, `make_in/`,
  `Makefile`, `.github/`.
- Out of scope: SPTAG containers, benchmarks.

Check file (read-only): `docs/checks/simd-vector-search/008-server-linux-image.md`
Report path: `docs/jobs/simd-vector-search/008-server-linux-image-01.md`
Duration hint: image build ~15-40m. Slow docker builds are not stalls.

<!-- architect-run: simd-vector-search -->

## Comments
