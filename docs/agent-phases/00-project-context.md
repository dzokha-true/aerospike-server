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
- **Phase 3 (shipped):** `VECTOR_DISTANCE` — SPTAG-compatible tail distance on listed Head IDs, owner-local top-K per request
- **Does not own:** `graph.bin`, RNG edges, BK-tree, graph traversal
- **CE note:** `AS_PARTICLE_TYPE_VECTOR` still uses the blob vtable; distance is a separate EC528 batch path, not a particle-type feature

---

## Query paths

### Baseline (SPTAG-only distance)

1. SPTAG: graph search on `m_pGraph` → head candidate IDs
2. SPTAG → Aerospike: MultiGet posting bins by head ID
3. SPTAG: `ComputeDistance` on tail bytes in postings
4. SPTAG: merge top-K

### Target (Phase 3 server + Phase 4 client)

1. SPTAG: graph search (unchanged)
2. SPTAG: parallel `VECTOR_DISTANCE` per owning node (`AS_MSG_INFO1_BATCH` + field **44**)
3. Aerospike: Owner-Local Top K + per-key statuses (field **45**)
4. SPTAG: global top-K merge (unchanged)

---

## Glossary


| Term                | Meaning                                                      |
| ------------------- | ------------------------------------------------------------ |
| **Head graph**      | In-RAM RNG/BKT/KDT structures in SPTAG                       |
| **Head ID**         | Aerospike record key for one head’s posting list             |
| **Posting list**    | Bin blob: packed tail vectors + metadata for one head        |
| **Tail vector**     | One embedding inside a posting; distance target              |
| **AEROSPIKEIO**     | SPTAG storage mode: postings in Aerospike, graph on disk/RAM |
| **ComputeDistance** | SPTAG reference distance on posting bytes (baseline / A-B) |
| **VECTOR_DISTANCE** | EC528 batch field 44/45; see `docs/contracts/sptag-aerospike.md` |


---

## Repo map (high level)


| Path                      | Role                                           |
| ------------------------- | ---------------------------------------------- |
| `as/include/fabric/hb.h`  | `AS_CLUSTER_SZ` (default **32**, EC528)        |
| `as/include/base/proto.h` | `AS_MSG_OP_*` and EC528 field types 44/45      |
| `as/src/base/batch.c`     | `VECTOR_DISTANCE` dispatch to vector handler   |
| `as/src/vector/`          | Posting parser, distance, wire codec, top-K      |
| `as/src/base/particle.c`  | Particle types; VECTOR uses blob vtable        |
| `as/src/base/cfg.c`       | Namespace vector config parsing                |
| `as/src/geospatial/`      | Precedent for C++ feature module in server     |
| `as/src/transaction/`     | Read/write transaction path (not vector entry) |
| `docs/architecture/module-map.md` | Request-flow diagram for agents        |


Build: see root `README.md`, `./bin/install-dependencies.sh`. Vector unit tests: `make -C as run-vector-tests`.

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
