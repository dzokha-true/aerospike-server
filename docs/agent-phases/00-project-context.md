# Project context (read once)

Use this as background for any phase. Phase-specific files repeat a short version.

---

## What we are building

**Goal:** Run [Microsoft SPTAG](https://github.com/microsoft/SPTAG) vector search with **low p99 latency**, **high QPS**, and **durability**, using a **forked Aerospike server** as the storage backend.

**Not the goal (unless explicitly redesigned later):**

- Storing or traversing the ANN graph inside Aerospike
- Replacing SPTAG approximate search with server-side brute force
- Lua UDFs on the hot query path (already tried — too slow)

---

## Two systems, one query

### SPTAG (smart client)

- Processes: AnnService, ssdserving, benchmark — anything calling `VectorIndex::LoadIndex`
- **Owns at query time:** head ANN graph in RAM
  - `RelativeNeighborhoodGraph` inside `BKT::Index` or `KDT::Index`
  - Loaded from index folder: `graph.bin`, BK-tree, head vectors
  - Coarse search: `m_pGraph` traversal (local only)
- **Does not own:** durable posting storage when `Storage=AEROSPIKEIO`

### Aerospike (storage backend — this repo)

- **Owns:** posting-list blobs keyed by **head ID** (`AEROSPIKEIO` mode)
- **New capability we add:** native C++ distance on tail vectors inside posting bins, per partition
- **Does not own:** `graph.bin`, RNG edges, BK-tree, graph traversal
- **CE today:** `AS_PARTICLE_TYPE_VECTOR` is a blob alias — no distance math, no ANN

---

## Query paths

### Baseline (today)

1. SPTAG: graph search on `m_pGraph` → head candidate IDs
2. SPTAG → Aerospike: MultiGet posting bins by head ID
3. SPTAG: `ComputeDistance` on tail bytes in postings
4. SPTAG: merge top-K

### Target (after Phase 3–4)

Steps 1–2 unchanged. Step 3 becomes: Aerospike `VECTOR_DISTANCE` on owning nodes; SPTAG merges results.

---

## Glossary


| Term                | Meaning                                                      |
| ------------------- | ------------------------------------------------------------ |
| **Head graph**      | In-RAM RNG/BKT/KDT structures in SPTAG                       |
| **Head ID**         | Aerospike record key for one head’s posting list             |
| **Posting list**    | Bin blob: packed tail vectors + metadata for one head        |
| **Tail vector**     | One embedding inside a posting; distance target              |
| **AEROSPIKEIO**     | SPTAG storage mode: postings in Aerospike, graph on disk/RAM |
| **ComputeDistance** | SPTAG client function on posting bytes (to offload)          |


---

## Repo map (high level)


| Path                      | Role                                           |
| ------------------------- | ---------------------------------------------- |
| `as/include/fabric/hb.h`  | `AS_CLUSTER_SZ` (default 8) — cluster node cap |
| `as/include/base/proto.h` | Wire op codes (`AS_MSG_OP_`*)                  |
| `as/src/base/particle.c`  | Particle types; VECTOR uses blob vtable        |
| `as/src/base/cfg.c`       | Namespace config parsing                       |
| `as/src/geospatial/`      | Precedent for C++ feature module in server     |
| `as/src/query/query.c`    | Background query / aggregation                 |
| `as/src/transaction/`     | Read/write transaction path                    |


Build: see root `README.md`, `./bin/install-dependencies.sh`.

---

## How agents work here (spec + test driven)

1. Read behavior spec in `docs/specs/` and contract in `docs/contracts/`
2. Write a **failing** test citing `SPEC-*-NNN`
3. Implement minimal code → green → refactor
4. Mark spec `verified` with test path

Full rules: [00-spec-and-test-driven.md](./00-spec-and-test-driven.md)

---

## Failed approaches (do not repeat)

- **Lua UDF distance** — interpreter overhead, poor p99/QPS  
- **Policy knob tuning only** — no server primitive for distance

---

## Fork convention

Mark all local deltas: `// EC528: <reason>`

Do not remove upstream copyright headers. AGPL applies to distributed binaries.