# ADR 0003: Cluster Size Limit

## Status

Accepted

## Context

Aerospike CE currently defaults `AS_CLUSTER_SZ` to `8` in `as/include/fabric/hb.h`. The value is used in fixed-size cluster and partition ownership structures.

Phase 2 needs larger clusters for SPTAG throughput experiments and partition fanout.
Those experiments need at least 9 nodes, but do not require fully dynamic
cluster sizing.

The cluster size is not just a runtime knob. `as/include/fabric/partition_balance.h` enforces that `AS_CLUSTER_SZ` is a power of two, and multiple arrays are sized by `AS_CLUSTER_SZ`.

## Decision

Raise the default CE compile-time cluster size to `AS_CLUSTER_SZ=32`.

`32` is the Phase 2 target because it:

- Allows clusters larger than the former 8-node cap.
- Preserves the existing power-of-two partition-balance invariant.
- Keeps fixed-array and partition-balance table growth bounded for benchmark
  work.

Phase 2 must preserve:

- Power-of-two constraint.
- Fixed-array bounds.
- Roster and succession list validation.
- Replication factor validation.
- Partition balance table sizing.

Clusters with more than 8 nodes should use mesh heartbeat mode. Multicast
heartbeat remains limited by multicast packet/adjacency constraints and is not
the required path for Phase 2 larger-cluster testing.

## Consequences

- Agents must not change `AS_CLUSTER_SZ` casually while working on vector distance.
- Tests or reviews must include code paths that assume `AS_CLUSTER_SZ` array bounds.
- `g_full_node_seq_table` and `g_full_sl_ix_table` grow linearly with
  `AS_CLUSTER_SZ * AS_PARTITIONS`; this is acceptable at 32, but values such as
  128 need separate memory review.
- Mesh heartbeat configuration is the documented path for 9+ node validation.

## Alternatives Considered

### Ignore Cluster Size During Vector Work

Rejected. Phase 3/4 performance experiments may depend on larger clusters, and silently retaining the cap can invalidate benchmark plans.

### Make Cluster Size Fully Dynamic Immediately

Deferred. Fully dynamic cluster sizing is broader than Phase 2 and touches many fixed-size structures.

### Use Non-Power-Of-Two Cluster Size

Rejected. Current partition balancing code asserts a power-of-two cluster size.

### Raise Directly To 64 Or Higher

Rejected for Phase 2. Larger values may be useful later, but they increase
fixed array and partition-balance table memory without being required for the
current 9+ node benchmark goal.
