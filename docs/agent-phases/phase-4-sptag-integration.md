# Phase 4 — SPTAG integration and benchmark

**Repos:**

1. **SPTAG** (Microsoft) — smart client, graph, `ComputeDistance`
2. **Aerospike server fork** (this repo) — postings + `VECTOR_DISTANCE` from Phase 3
3. **Aerospike C client fork** — send new proto ops (may be separate task)

**Depends on:** Phase 3 ops live; Phase 1 contract doc; Phase 2 if testing ≥9 nodes  
**Blocks:** nothing (end of restart track)

---

## Project context (read first)

### Storage mode: `AEROSPIKEIO`

- Head graph + `graph.bin` → SPTAG process disk/RAM
- Posting lists → Aerospike bins, key = **head ID**

### Query pipeline

| Step | Owner | Phase 4 change |
|------|-------|----------------|
| Load index | SPTAG | none |
| `m_pGraph` search | SPTAG | **none** |
| MultiGet postings | Aerospike client | maybe keep |
| Tail distance | ~~SPTAG `ComputeDistance`~~ | **Aerospike `VECTOR_DISTANCE`** |
| Top-K merge | SPTAG | none |

### Restarts

- **SPTAG restart:** reload index folder → graph back in RAM
- **Aerospike restart:** postings persist; graph search unchanged

---

## Your mission

1. Document posting byte layout from SPTAG source (if not done in Phase 1)
2. Replace hot-path `ComputeDistance` with Aerospike `VECTOR_DISTANCE` fanout
3. Benchmark vs baseline; report p99, QPS, CPU
4. Keep Merge/Split on posting writes unless broken

---

## Spec-driven + test-driven (this phase)

1. Read `docs/specs/phase-4-integration.md`
2. **Red:** automated test for `SPEC-4-INTEG-001` (top-K parity vs `ComputeDistance` with flag off) — must fail until SPTAG wired
3. **Green:** wire `VECTOR_DISTANCE`, flip flag, test passes
4. **Benchmark spec** `SPEC-4-BENCH-001`: document methodology in spec; results in `docs/benchmarks/phase-4-results.md` (not a substitute for INTEG test)

Correctness before performance.

---

## Prerequisites checklist

Before coding, verify:

- [ ] `docs/contracts/sptag-aerospike.md` has posting layout matching SPTAG `AEROSPIKEIO` write path
- [ ] Server built with Phase 3 ops
- [ ] Forked C client can send `AS_MSG_OP_VECTOR_DISTANCE` (or use test harness in server repo)
- [ ] Test index built with `Storage=AEROSPIKEIO`

---

## Step 1 — Posting layout (SPTAG repo)

Find and document (paste into contract doc):

- Where postings are written to Aerospike (file/class names)
- `ComputeDistance` implementation — input buffer layout
- Head ID key format, namespace/set/bin names
- Float dtype (expect `float32` LE), dimension source

**Search hints in SPTAG:** `AEROSPIKEIO`, `ComputeDistance`, `VectorIndex`, posting, MultiGet, aerospike.

---

## Step 2 — Wire SPTAG query path

Pseudocode target:

```
candidates = index->SearchGraph(query)   // unchanged, local m_pGraph
head_ids = extract_head_ids(candidates)
// OLD: postings = MultiGet(head_ids); for (p : postings) ComputeDistance(query, p)
// NEW:
distances = aerospike_vector_distance_parallel(head_ids, query_vector)
merged = sptag_merge_topk(candidates, distances)
```

**Parallel fanout:** group `head_ids` by Aerospike partition → one request per node (match Phase 3 design).

**Feature flag:** e.g. env `SPTAG_AS_VECTOR_DISTANCE=1` to A/B vs `ComputeDistance` baseline.

Do **not** move `m_pGraph` to server.

---

## Step 3 — C client (if in scope)

If this agent only has server repo: write `docs/contracts/client-vector-distance.md` with:

- Op code number from Phase 3
- Serialized request/response (field order, types)
- Example pseudocode for parallel batch

If agent has client repo: implement op registration + send/receive matching contract.

---

## Step 4 — Benchmark

Compare **baseline** vs **VECTOR_DISTANCE**:

| Metric | How |
|--------|-----|
| p99 latency | same QPS, measure end-to-end search |
| QPS | ramp until p99 degrades |
| SPTAG CPU | `top` / perf |
| Aerospike CPU | per-node |
| Correctness | top-K overlap vs baseline (allow tie ordering diff) |

Document: dataset, dim, node count, RF, mesh vs multicast, hardware.

Use Phase 2 cluster if testing &gt;8 nodes.

---

## Step 5 — Merge / Split

Only if required for your index build:

- Posting writes should still work via existing Aerospike put API
- Graph Merge/Split stays in SPTAG — no server graph ops

---

## Acceptance criteria

- [ ] `SPEC-4-INTEG-001` automated test green (top-K parity threshold in spec)
- [ ] `SPEC-4-BENCH-001` results recorded per spec methodology
- [ ] Posting layout in contract doc matches SPTAG bytes (reviewed)
- [ ] Search/MultiSearch hot path uses `VECTOR_DISTANCE` behind flag
- [ ] Baseline flag still runs `ComputeDistance` for A/B
- [ ] Benchmark table in `docs/benchmarks/phase-4-results.md` (create)
- [ ] No regression: index load, graph search, posting persistence
- [ ] Advisor one-liner true: “SPTAG owns ANN; Aerospike owns postings + tail distance”

---

## Anti-patterns

- Removing `ComputeDistance` without baseline flag
- Storing `graph.bin` in Aerospike “for convenience”
- Single-node distance loop from one SPTAG thread (must fan out)
- Claiming success without correctness check vs baseline top-K

---

## Failure modes to debug

| Symptom | Check |
|---------|-------|
| Wrong distances | posting parser vs SPTAG layout; metric mismatch |
| Missing keys | head ID format; partition routing |
| Slower than baseline | batch size; too many round trips; no parallel per node |
| p99 spikes | hot partition; network; serial merge |

---

## Out of scope

- Server-side graph replication
- Production hardening (auth, quotas) unless asked
- Upstreaming to Aerospike Inc.

---

## After you finish

Project restart track complete. Future work: optimize batch sizes, min-per-head vs per-tail, optional `VECTOR_BATCH_GET`, larger `AS_CLUSTER_SZ`.
