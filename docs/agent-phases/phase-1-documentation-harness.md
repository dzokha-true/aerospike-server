# Phase 1 — Documentation harness

**Repo:** Aerospike server CE fork (`aerospike-server-upstream`)  
**Depends on:** nothing  
**Blocks:** Phases 2–4 (agents need these docs to avoid wrong assumptions)

---

## Project context (read first)

You are documenting a **fork of Aerospike** used as the **storage backend for Microsoft SPTAG** when `Storage=AEROSPIKEIO`.

| System | Query-time role |
|--------|-----------------|
| **SPTAG** | Holds head ANN graph in RAM (`graph.bin`, RNG, BKT/KDT); runs `m_pGraph` search |
| **Aerospike** | Stores **posting-list blobs** keyed by **head ID**; will later compute **tail distance** natively |

**Baseline query:** SPTAG graph search → MultiGet postings → client `ComputeDistance` → top-K merge.  
**Target query:** same, but tail distance moves to Aerospike (Phase 3).

**Out of scope for all phases:** server-side graph traversal, HNSW inside Aerospike, Lua UDF hot path.

---

## Your mission

Create a **small, accurate doc tree** so future coding agents (and humans) can navigate this C++/cmake codebase without reading the entire tree.

**Do not** implement server features in this phase — documentation, **specs**, and **doc tests** only.

---

## Spec-driven + test-driven (this phase)

Read [00-spec-and-test-driven.md](./00-spec-and-test-driven.md) first.

Phase 1 **is** TDD — tests are doc/spec conformance, not C++ yet:

1. **Spec-first:** draft all behavior specs for Phases 2–4 before any later phase starts coding
2. **Test-first:** create `tests/docs/check_docs.sh` that fails until deliverables exist
3. **Red → green:** run script (must fail), add files, run again (must pass)

---

## Deliverables (create these files)

### 1. `CONTEXT.md` (repo root)

Domain glossary only — **no implementation details**. Use this structure:

```markdown
# SPTAG + Aerospike

One paragraph: SPTAG smart client + Aerospike posting storage.

## Language

**Smart Client**: ...
**Head Graph**: ...
(etc.)

## Relationships

- One **Head ID** maps to one **Posting List** in Aerospike
- **Head Graph** search happens only in **Smart Client**

## Example dialogue

> Dev: ...
> Expert: ...

## Flagged ambiguities

(none initially, or list open items)
```

**Required terms:** Smart Client, Head Graph, Head ID, Posting List, Tail Vector, AEROSPIKEIO, ComputeDistance, Partition-Local Distance.

### 2. `AGENTS.md` (repo root)

Audience: agents who do not know C++ or cmake. Include:

| Section | Content |
|---------|---------|
| **Goal** | One paragraph linking to CONTEXT.md |
| **Build** | `./bin/install-dependencies.sh`, main build commands from `README.md` |
| **Directory map** | `as/` (server), `cf/` (common), key subdirs |
| **Hot paths** | `transaction/`, `query/query.c`, `base/particle*.c`, `fabric/`, `sindex/` |
| **Invariants** | `AS_CLUSTER_SZ` must be power of 2; VECTOR particle = blob vtable |
| **Fork markers** | `// EC528:` on all local changes |
| **Do not touch** | EE-only code paths without explicit task |
| **Phase index** | Link to `docs/agent-phases/` |

### 3. `docs/contracts/sptag-aerospike.md`

Wire contract between SPTAG and this fork. Include:

**Ownership table** (who owns graph vs postings vs distance).

**Query stage table:**

| Stage | SPTAG | Aerospike today | Aerospike target |
|-------|-------|-----------------|------------------|
| Coarse search | `m_pGraph` | — | — |
| Fetch postings | — | MultiGet by head ID | MultiGet / batch get |
| Tail distance | `ComputeDistance` | — | `AS_MSG_OP_VECTOR_DISTANCE` |
| Top-K | client merge | — | client merge |

**Posting bin format:**

- Placeholder section: `## Posting byte layout (from SPTAG)` with `TODO: fill from SPTAG AEROSPIKEIO source`
- Namespace knobs: `vector-dimension`, `vector-metric {l2|cosine|dot}`
- Keys = head ID; bin names as SPTAG defines

**Out of contract:** `graph.bin`, RNG adjacency, BK-tree on server.

**Opcode placeholders:** `AS_MSG_OP_VECTOR_DISTANCE`, `AS_MSG_OP_VECTOR_BATCH_GET` (IDs TBD in Phase 3).

### 4. `docs/architecture/module-map.md`

One page with mermaid:

```
Client → proto.h → transaction → storage → particle
```

Annotate where Phase 3 will add `as/src/vector/`.

### 5. `docs/adr/0001-hybrid-sptag-storage-split.md`

ADR: SPTAG owns ANN graph; Aerospike owns postings + future tail distance.  
Include: alternatives considered (full ANN on server, UDF-only), why rejected.

### 6. `docs/adr/0002-vector-bin-format.md` (stub)

Status: proposed. Link to contract doc posting section. Fill when SPTAG layout known.

### 7. `docs/adr/0003-cluster-size-limit.md` (stub)

Status: proposed. Note `AS_CLUSTER_SZ=8` in `as/include/fabric/hb.h`, power-of-2 requirement. Phase 2 implements.

### 8. Optional: `CONTEXT-MAP.md`

Only if SPTAG lives in another repo — point to its path/URL and say server contract is in `docs/contracts/`.

### 9. Behavior specs (draft — required)

Create under `docs/specs/` from `_template.md`:

| File | Example spec IDs |
|------|------------------|
| `phase-2-cluster.md` | `SPEC-2-CLUSTER-001` (9 nodes mesh join), `SPEC-2-CLUSTER-002` (RF ≤ AS_CLUSTER_SZ) |
| `phase-3-vector-distance.md` | `SPEC-3-VDIST-001` (L2 reference), `SPEC-3-PARSER-001` (synthetic posting), `SPEC-3-OP-001` (VECTOR_DISTANCE) |
| `phase-4-integration.md` | `SPEC-4-INTEG-001` (top-K parity vs ComputeDistance), `SPEC-4-BENCH-001` (p99/QPS methodology) |

Status: `draft`. Later phases set `verified` + `tests: [...]`.

### 10. `tests/docs/check_docs.sh`

Executable script. Exit non-zero if any missing:

- `CONTEXT.md`, `AGENTS.md`
- `docs/contracts/sptag-aerospike.md`
- `docs/specs/phase-{2,3,4}-*.md`
- `docs/adr/0001-*.md`
- Internal links in `AGENTS.md` resolve

Document in `AGENTS.md`: `./tests/docs/check_docs.sh`

### 11. `AGENTS.md` section: Spec + TDD

Mandate: read `00-spec-and-test-driven.md`; cite `SPEC-*` in tests; red-green-refactor for all phases.

---

## Reference files to read (not modify)

| File | Why |
|------|-----|
| `README.md` | Build steps |
| `as/include/fabric/hb.h` | `AS_CLUSTER_SZ` |
| `as/include/base/datamodel.h` | `AS_PARTICLE_TYPE_VECTOR` |
| `as/src/base/particle.c` | VECTOR → blob vtable |
| `as/include/base/proto.h` | Existing `AS_MSG_OP_*` pattern |
| `as/src/geospatial/geospatial.cc` | C++ module precedent |

---

## Acceptance criteria

- [ ] `./tests/docs/check_docs.sh` exits 0
- [ ] `docs/specs/phase-{2,3,4}-*.md` exist with ≥2 spec IDs each
- [ ] New agent can answer “who owns the ANN graph?” from `CONTEXT.md` in &lt;2 min
- [ ] `AGENTS.md` lists build command and 5+ important paths
- [ ] Contract doc has stage table + explicit out-of-scope list
- [ ] ADR 0001 documents hybrid split with trade-offs
- [ ] No server C++ behavior changes in this phase (docs only)
- [ ] All new docs use full paths or repo-relative links

---

## Anti-patterns

- Putting C++ struct layouts in `CONTEXT.md` (glossary only)
- Documenting Lua UDF as recommended hot path
- Claiming Aerospike already has vector distance (it does not in CE)

---

## After you finish

Tell the human: Phase 2 and 3 agents should read `docs/contracts/sptag-aerospike.md` before coding. Posting byte layout must be filled from SPTAG repo before Phase 3 parser work is final.
