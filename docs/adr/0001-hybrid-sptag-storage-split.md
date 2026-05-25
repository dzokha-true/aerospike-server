# ADR 0001: Hybrid SPTAG Storage Split

## Status

Accepted

## Context

SPTAG already owns ANN index construction, graph files, in-memory graph traversal, candidate Head ID selection, and global top-K merge. In `AEROSPIKEIO` mode, Aerospike is used for durable posting-list storage keyed by Head ID.

This fork is meant to reduce hot-path tail distance cost while preserving SPTAG's graph ownership.

## Decision

Keep a hybrid split:

- SPTAG owns Head Graph search and global merge.
- Aerospike owns posting-list records keyed by Head ID.
- Aerospike computes Tail Vector distances for explicitly listed Head ID records on the owner node (`VECTOR_DISTANCE`, Phase 3).

Aerospike will not store or traverse the ANN graph.

## Consequences

- SPTAG remains the source of truth for graph semantics, posting layout, ValueType, Dimension, and DistMethod.
- Aerospike server changes can stay focused on storage, key lookup, posting decode, and distance math.
- Query semantics remain key-scoped: no partition scan, namespace scan, or server-side brute-force ANN replacement.
- Phase 3 and Phase 4 must keep client/server config in sync.

## Alternatives Considered

### Full ANN On Aerospike Server

Rejected. It would move graph ownership, graph persistence, graph traversal, and ANN index lifecycle into Aerospike. That is a larger redesign and duplicates SPTAG's core responsibility.

### Server-Side Brute Force

Rejected. Scanning records or partitions for every query has the wrong cost model and does not use SPTAG's coarse graph search.

### Lua UDF Distance

Rejected. Lua UDFs on the hot path add interpreter overhead and already failed the latency/QPS goal.

### Policy Knob Tuning Only

Rejected. Tuning cannot create a native server primitive for key-scoped tail distance.
