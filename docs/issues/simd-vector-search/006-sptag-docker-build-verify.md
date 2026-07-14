---
issue: 6
title: "sptag B2: Docker build with forked client; offload test suite runs real"
state: OPEN
parent: 1
blocked-by: none
---
Repo: **SPTAG-upstream**, branch factory/simd-vector-search (base 1b4f9b2).
This job runs in a SPTAG-upstream worktree, NOT in the coordination repo.

## What to build

The current `Dockerfile` installs the STOCK Aerospike C client 7.3.0 from
aerospike.com (x86_64 tarball), so the CMake probe for
`aerospike/aerospike_vector_distance.h` can never succeed and the image is
broken on arm64. Fix it so the offload build is real:

1. Rework `Dockerfile` to build and install the forked client from
   `https://github.com/dzokha-true/aerospike-client-c.git` (master; commit
   9ae3eda3 has the VECTOR_DISTANCE wiring) instead of the stock tarball.
   The client build needs its own submodules (`git clone --recursive`) and
   works on arm64 Linux. Keep ubuntu:20.04 + gcc-8 unless something genuinely
   blocks; record any base-image change as a PHASE-0 disagreement.
2. The SPTAG CMake probe (`CMakeLists.txt:182-217`) must succeed:
   `SPTAG_HAS_AEROSPIKE_VECTOR_DISTANCE` defined. Make the image write
   `/app/build/offload_probe_ok` containing exactly
   `SPTAG_HAS_AEROSPIKE_VECTOR_DISTANCE=1` during the build when (and only
   when) the probe passed; a probe failure must fail the docker build loudly.
3. Add `/app/run-offload-tests.sh` (committed as `Script_AE/run-offload-tests.sh`
   or a sensible repo path, COPY'd into the image): runs the Boost.Test
   `VectorDistanceOffloadTest` suite from the built test binary and exits
   nonzero on failure. Output must include Boost's "No errors detected" on
   success.
4. Image must build with plain `docker build -t <tag> .` on this arm64 host
   with network available.

## Acceptance criteria

- `docker build` succeeds on arm64; probe marker present with the exact
  content above; offload test suite runs real (compiled against the forked
  client headers) and passes.
- Any client-c source defect discovered is a BLOCKED report with exact
  compiler/linker output — do not patch the client repo from this job.

## Boundaries

- MAY TOUCH (SPTAG-upstream only): `Dockerfile`, `Script_AE/`,
  `docs/contracts/`, `CMakeLists.txt` and `AnnService/CMakeLists.txt` (probe
  fixes only), `Test/CMakeLists.txt` (only if the test target needs it).
- MUST NOT TOUCH: `docs/checks/`, `AnnService/inc|src` implementation files,
  `Test/src/*.cpp`, `datasets/`, the aerospike-client-c repo, this
  coordination repo.
- Out of scope: quantizer guard (#7), integration parity (#9).

Check file (read-only, in SPTAG-upstream):
`docs/checks/simd-vector-search/006-sptag-docker-build-verify.md`
Report path (in SPTAG-upstream):
`docs/jobs/simd-vector-search/006-sptag-docker-build-verify-01.md`
Duration hint: first image build ~30-60m (apt + client + SPTAG compile).
Slow docker builds are not stalls.

<!-- architect-run: simd-vector-search -->

## Comments
