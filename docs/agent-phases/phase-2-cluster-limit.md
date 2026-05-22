# Phase 2 — Raise 8-node cluster limit

**Repo:** Aerospike server CE fork  
**Depends on:** Phase 1 recommended (read `AGENTS.md`, ADR stub 0003)  
**Blocks:** Large-scale multi-node benchmarks in Phase 4

---

## Project context (read first)

This fork supports **SPTAG + Aerospike** (`AEROSPIKEIO`): SPTAG holds the ANN graph in RAM; Aerospike stores posting lists by head ID. Phase 3 adds tail distance on servers.

**This phase is unrelated to vectors** — it only removes the **8-node cluster cap** so experiments can run more nodes for QPS and partition fanout.

---

## Problem

Aerospike CE hard-limits cluster size to **8 nodes** via `AS_CLUSTER_SZ`:

```c
// as/include/fabric/hb.h
#ifndef AS_CLUSTER_SZ
#define AS_CLUSTER_SZ 8
#endif
```

This constant sizes **dozens of fixed arrays** across fabric, partitions, namespaces, exchange, etc. Multicast heartbeat also caps adjacency at `ASC = 8` in `as/src/fabric/hb.c`.

**Mesh heartbeat** mode uses `INT_MAX` for `config_mcsize()` — preferred for clusters &gt;8.

---

## Your mission

Raise the compile-time cluster limit to **`AS_CLUSTER_SZ=32`** (power of 2). Audit and fix dependent code. Document in ADR 0003. Validate cluster can form with &gt;8 nodes in mesh mode.

**Do not** change vector/distance code in this phase.

---

## Spec-driven + test-driven (this phase)

1. Read `docs/specs/phase-2-cluster.md` — implement **only** listed `SPEC-2-*` scenarios
2. **Red:** add failing test/script (e.g. `tests/integration/cluster_mesh_9nodes.sh` or gtest if you add harness) citing spec IDs
3. **Green:** raise `AS_CLUSTER_SZ`, fix audit issues until test passes
4. Update spec frontmatter: `status: verified`, `tests: [...]`

No spec ID → no code change.

---

## Constraints

| Rule | Reason |
|------|--------|
| `AS_CLUSTER_SZ` **must stay power of 2** | `COMPILER_ASSERT` in `as/include/fabric/partition_balance.h` |
| Prefer **mesh** HB for &gt;8 nodes | Multicast `ASC=8` rejects adjacency beyond 8 |
| Tag edits `// EC528:` | Fork traceability |
| No unrelated refactors | Keep review small |

---

## Implementation steps

### 1. Set cluster size

Either:

- Build flag: `-DAS_CLUSTER_SZ=32` in cmake/build docs, **or**
- Change default in `as/include/fabric/hb.h` to 32 with `// EC528:` comment

Default **32** unless human specifies 64.

### 2. Audit `AS_CLUSTER_SZ` usages

Search repo for `AS_CLUSTER_SZ` and literal `8` in cluster-sizing context.

**Known hot files:**

| File | Risk |
|------|------|
| `as/include/base/datamodel.h` | `succession[AS_CLUSTER_SZ]`, roster arrays |
| `as/include/fabric/partition.h` | replica arrays |
| `as/src/fabric/partition_balance.c` | `g_full_node_seq_table[AS_CLUSTER_SZ * AS_PARTITIONS]` — memory ∝ N |
| `as/src/fabric/fabric.c` | node list caps |
| `as/src/fabric/exchange.c` | roster `> AS_CLUSTER_SZ` reject |
| `as/src/fabric/hb.c` | `ASC`, adjacency limits (mesh vs multicast) |
| `as/src/base/cfg.c` | `min-cluster-size`, replication-factor caps, mesh seeds |
| `as/src/base/health.c` | `MAX_NODES_TRACKED = 2 * AS_CLUSTER_SZ` |

Fix any **hardcoded 8** that should track `AS_CLUSTER_SZ`.

### 3. Mesh heartbeat documentation

In ADR 0003 or `AGENTS.md`, note: for N&gt;8, aerospike.conf should use **mesh** not multicast.

### 4. Memory sanity

`g_full_node_seq_table` size grows with N. At N=32 this is acceptable; note in ADR if N=128+ needs review.

### 5. Complete `docs/adr/0003-cluster-size-limit.md`

Status: accepted. Document chosen N, power-of-2 rule, mesh requirement, memory trade-off.

---

## Validation (required)

Tests must map to `SPEC-2-CLUSTER-*` in PR description.

Document how you verified (commands in PR notes):

1. **Build** server with new `AS_CLUSTER_SZ`
2. **Config:** mesh + ≥9 seed addresses (or docker-compose if available)
3. **Cluster forms:** `asadm` / info shows expected node count
4. **No crash** on `cfg_add_mesh_seed_addr_port` with &gt;8 seeds (see `cfg.c` ~5569)
5. **Replication factor** configurable up to `AS_CLUSTER_SZ`

If you cannot run multi-node locally, list exact manual test steps for the human.

---

## Acceptance criteria

- [ ] `AS_CLUSTER_SZ` is 32 (or documented alternative power of 2)
- [ ] Audit list in PR: files touched + any hardcoded 8 fixed
- [ ] ADR 0003 complete
- [ ] Build succeeds
- [ ] No vector/proto changes
- [ ] `// EC528:` on functional deltas

---

## Anti-patterns

- Setting N=10 or 12 (not power of 2 — build fail)
- Only changing `hb.h` without auditing arrays
- Assuming multicast works for 16 nodes without mesh

---

## Out of scope

- Enterprise feature keys (`g_hb_cluster_nodes_limit` — unused in CE)
- Vector ops (Phase 3)

---

## After you finish

Phase 4 benchmarks may use ≥9 node clusters. Phase 3 does not depend on this phase but benefits from it.
