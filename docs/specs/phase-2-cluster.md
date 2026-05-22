---
id: SPEC-2-CLUSTER-001
phase: 2
status: verified
contract: docs/adr/0003-cluster-size-limit.md
tests:
  - tests/integration/cluster_mesh_9nodes.sh
---

# Phase 2 Cluster Size Limit

This spec covers the Phase 2 cluster-size increase only. It does not change
vector storage, vector distance, SPTAG graph ownership, or query semantics.

## Verification

- `tests/integration/cluster_mesh_9nodes.sh` passes locally.
- `bash -n tests/integration/cluster_mesh_9nodes.sh` passes locally.
- Full local `make -j4` was attempted on macOS, but this server build reports
  `darwin is not supported`; CI Linux build remains the required full build
  verification path.

## SPEC-2-CLUSTER-001: Mesh Cluster Allows 9 Nodes

### Given

- The server is built with the Phase 2 cluster-size limit.
- Heartbeat mode is `mesh`.
- A cluster configuration contains 9 nodes that can reach each other through
  mesh heartbeat addresses.

### When

- All 9 server processes start and join the same cluster.

### Then

- The cluster accepts all 9 nodes.
- Cluster membership/info output reports 9 nodes.
- No node rejects membership because of the former 8-node cap.

### Error cases

- If heartbeat mode is multicast, this spec does not require more than 8 nodes
  to be accepted.
- If fewer than 9 nodes are reachable, the cluster may report only the reachable
  node count.

## SPEC-2-CLUSTER-002: Mesh Seeds Accept More Than 8 Entries

### Given

- Heartbeat mode is `mesh`.
- The configuration contains 9 `mesh-seed-address-port` entries.

### When

- The server parses the configuration.

### Then

- Configuration parsing succeeds.
- The 9th seed address is accepted.
- The server does not crash with `can't configure more than 8
  mesh-seed-address-port entries`.

### Error cases

- Configurations with more than `AS_CLUSTER_SZ` mesh seed entries may be
  rejected.
- `mesh-seed-address-port` remains invalid in multicast heartbeat mode.

## SPEC-2-CLUSTER-003: Runtime Cluster Knobs Use The Raised Limit

### Given

- The server is built with `AS_CLUSTER_SZ=32`.

### When

- `min-cluster-size` is configured with a value from 1 through 32.
- `replication-factor` is configured with a value from 1 through 32.
- Dynamic `replication-factor` info config is requested with a value from 1
  through 32 for a non-strong-consistency namespace.

### Then

- Values in the range 1 through 32 are accepted by the same validation paths
  that previously capped them at 8.
- Values greater than 32 are rejected.

### Error cases

- `replication-factor` values less than 1 are rejected.
- Strong-consistency namespaces keep existing restrictions on dynamic
  `replication-factor` changes.

## SPEC-2-CLUSTER-004: Cluster Size Remains Power Of Two

### Given

- `AS_CLUSTER_SZ` controls fixed cluster arrays and partition-balance bit masks.

### When

- The server is compiled with the Phase 2 default cluster size.

### Then

- `AS_CLUSTER_SZ` is 32.
- The build-time power-of-two assertion in partition balancing still passes.
- Cluster-size-dependent structures compile without breaking alignment asserts.

### Error cases

- Non-power-of-two values such as 10 or 12 are invalid for Phase 2.
