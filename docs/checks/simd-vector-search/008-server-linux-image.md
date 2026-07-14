# Check: 008 server Linux image + compose

Executor: bash
Spec: docs/spec/aerospike-simd-vector-search.md
Issue: docs/issues/simd-vector-search/008-server-linux-image.md
Duration hint: image build ~15-40m; not a stall.

## Runnable

- RUN: `docker build -f docker/Dockerfile -t as-vector:check . > .architect/tmp/db.log 2>&1; s=$?; tail -5 .architect/tmp/db.log; exit $s` -> exit 0, image builds from this source tree.
- RUN: `bash docker/smoke-up.sh > .architect/tmp/smoke.log 2>&1; s=$?; tail -6 .architect/tmp/smoke.log; exit $s` -> exit 0 and output contains `SERVER_VECTOR_READY`.
- RUN: `ls docker/compose-single.yml docker/compose-cluster3.yml docker/aerospike-vector.conf docker/README.md` -> all four paths exist; exit 0.
- RUN: `grep -n "AEROSPIKE_VECTOR_SIMD" docker/compose-single.yml docker/compose-cluster3.yml | head -4` -> SIMD env passthrough wired in both compose files; exit 0.

## Judge-only

- Dockerfile COPYs the local tree (submodules included), no network fetch of
  server source; base image and build steps sane for arm64.
- Vector namespace config carries dimension/value-type/metric with env
  defaults 64/float/l2 per the issue.
- Diff confined to `docker/` (plus the job report).
