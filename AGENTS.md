# Agent Guide

## Goal

This repo is an Aerospike Database Server CE fork used as the storage backend for Microsoft SPTAG `AEROSPIKEIO`. Read `CONTEXT.md` first: SPTAG owns the ANN graph and global merge; Aerospike owns posting-list records and owner-local `VECTOR_DISTANCE` on listed Head IDs (Phase 3).

## Build

- Install dependencies: `./bin/install-dependencies.sh`
- Fetch submodules: `git submodule update --init`
- Build server: `make`
- Parallel build: `make -j4`
- Package builds: `make deb` or `make rpm`
- Clean build outputs: `make clean`

The server build is GNU Make based. CMake is used for bundled C++ dependencies such as S2, Abseil, and JSON libraries.

## Directory Map

| Path | Role |
|------|------|
| `as/` | Aerospike server source, headers, and config examples |
| `as/src/base/` | Core runtime, protocol, config, service, particles, transaction setup |
| `as/src/transaction/` | Read/write/UDF transaction execution |
| `as/src/storage/` | Storage engines, record loading, flat record format |
| `as/src/fabric/` | Cluster membership, partition ownership, migration |
| `as/src/query/` | Background and query job execution |
| `as/src/sindex/` | Secondary index implementation |
| `as/src/geospatial/` | Existing C++ server feature precedent |
| `as/src/vector/` | EC528 posting parser, SPTAG distance, wire codec, batch handler |
| `as/include/vector/` | EC528 vector subsystem headers |
| `as/include/` | Server headers matching subsystem layout |
| `cf/` | Common shared library linked by server |
| `modules/` | Git submodules used by build |
| `make_in/` | Shared make variables, targets, and platform rules |
| `docs/agent-phases/` | Phase plans for this EC528 fork |

## Hot Paths

- Client protocol definitions: `as/include/base/proto.h`
- Socket ingress: `as/src/base/service.c`
- Transaction parsing: `as/src/base/transaction.c`
- Transaction dispatch: `as/src/base/thr_tsvc.c`
- Read path: `as/src/transaction/read.c`
- Read op bin handling: `as/src/transaction/rw_utils.c`
- Write op handling: `as/src/transaction/write.c`
- Batch parsing and subtransactions: `as/src/base/batch.c`
- `VECTOR_DISTANCE` dispatch: `as/src/base/batch.c` → `as/src/vector/vector_batch.c`
- Particle types: `as/include/base/datamodel.h`
- Particle vtables: `as/src/base/particle.c`
- Blob/vector byte handling: `as/src/base/particle_blob.c`
- Namespace config parsing: `as/src/base/cfg.c`
- Cluster size and heartbeat config: `as/include/fabric/hb.h`
- Partition balancing constraints: `as/include/fabric/partition_balance.h`

## Invariants

- SPTAG owns `graph.bin`, RNG/BKT/KDT graph state, coarse search, and global top-K merge.
- Aerospike stores posting-list blobs keyed by Head ID in `AEROSPIKEIO`.
- `AS_PARTICLE_TYPE_VECTOR` is a blob-backed particle type; tail distance uses `as/src/vector/` (scalar SPTAG-adapted math), not bin-op read hooks.
- `AS_CLUSTER_SZ` defaults to `32` (EC528 Phase 2) and must remain a power of two.
- `VECTOR_DISTANCE` is key-scoped: never scan a whole partition for ANN query work.
- Namespace vector config must match SPTAG index config before server-side distance can decode posting bytes.

## Fork Markers

Mark local C/C++ source deltas with:

```c
// EC528: <reason>
```

Do not remove upstream copyright headers. This repo is AGPL for distributed binaries.

## Do Not Touch Without Explicit Task

- Enterprise-only paths and CE stubs.
- Lua UDF hot path for vector distance.
- Server-side ANN graph traversal or graph storage.
- SPTAG graph ownership assumptions.

## Phase Index

- `docs/agent-phases/00-project-context.md`
- `docs/agent-phases/phase-1-documentation-harness.md`
- `docs/agent-phases/phase-2-cluster-limit.md`
- `docs/agent-phases/phase-3-vector-distance.md`
- `docs/agent-phases/phase-4-sptag-integration.md`

Phase 2 and Phase 3 agents must read `docs/contracts/sptag-aerospike.md` before coding.

Doc lint: `./tests/docs/check_docs.sh`
