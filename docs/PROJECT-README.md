# EC528: Server-Side, SIMD-Accelerated Vector Search in Aerospike — The Complete Story

This document explains, from absolute zero, what this project is, every piece
of technology it touches, every design decision that was made (and why), every
bug that was found (and how it was diagnosed), and every number that was
measured. It assumes you are a junior college graduate who has never touched
Aerospike, SPTAG, SIMD, or wire protocols. It is long on purpose.

The one-sentence summary, which the rest of this document unpacks:

> We taught the Aerospike database server to compute vector distances **next
> to the data it stores**, using **SIMD CPU instructions**, so that a vector
> search engine (Microsoft SPTAG) no longer has to drag gigabytes of raw
> vectors across the network per query — and we proved it end-to-end with a
> live 3-node cluster: **3.3× more queries per second and ~1000× less network
> traffic**, with answers that match the original system.

---

## Table of contents

- [Part I — The concepts, from zero](#part-i--the-concepts-from-zero)
  - [1. Vectors, embeddings, and nearest-neighbor search](#1-vectors-embeddings-and-nearest-neighbor-search)
  - [2. Why brute force fails, and how SPANN fixes it](#2-why-brute-force-fails-and-how-spann-fixes-it)
  - [3. What Aerospike is](#3-what-aerospike-is)
  - [4. What a "smart client" is](#4-what-a-smart-client-is)
  - [5. Near-data computing: the thesis of this project](#5-near-data-computing-the-thesis-of-this-project)
  - [6. What SIMD is](#6-what-simd-is)
  - [7. A crash course in wire protocols](#7-a-crash-course-in-wire-protocols)
- [Part II — The system as it existed before this run](#part-ii--the-system-as-it-existed-before-this-run)
  - [8. The three repositories](#8-the-three-repositories)
  - [9. The architecture contract (who does what)](#9-the-architecture-contract-who-does-what)
  - [10. The data formats, exactly](#10-the-data-formats-exactly)
  - [11. The distance math, exactly](#11-the-distance-math-exactly)
- [Part III — What this run built, issue by issue](#part-iii--what-this-run-built-issue-by-issue)
  - [12. Issue #2: the kernel dispatch table](#12-issue-2-the-kernel-dispatch-table)
  - [13. Issue #3: NEON kernels for ARM](#13-issue-3-neon-kernels-for-arm)
  - [14. Issue #4: SSE/AVX2/AVX-512 kernels + CPUID for x86](#14-issue-4-sseavx2avx-512-kernels--cpuid-for-x86)
  - [15. Issue #5: the parity suite and the floating-point tolerance story](#15-issue-5-the-parity-suite-and-the-floating-point-tolerance-story)
  - [16. Issue #6: making the SPTAG build real (and porting SPTAG to ARM)](#16-issue-6-making-the-sptag-build-real-and-porting-sptag-to-arm)
  - [17. Issue #7: the quantizer guard (a real bug found by review)](#17-issue-7-the-quantizer-guard-a-real-bug-found-by-review)
  - [18. Issue #8: a Docker image for the server (and the graveyard of latent bugs)](#18-issue-8-a-docker-image-for-the-server-and-the-graveyard-of-latent-bugs)
  - [19. Issue #10: the live wire smoke test](#19-issue-10-the-live-wire-smoke-test)
  - [20. Issue #9: end-to-end parity (and two great debugging stories)](#20-issue-9-end-to-end-parity-and-two-great-debugging-stories)
  - [21. Issue #11: the 3-node benchmark](#21-issue-11-the-3-node-benchmark)
  - [22. Issues #12/#13: documentation](#22-issues-1213-documentation)
- [Part IV — How the work itself was organized](#part-iv--how-the-work-itself-was-organized)
- [Part V — How to run everything yourself](#part-v--how-to-run-everything-yourself)
- [Part VI — Honest limitations and future work](#part-vi--honest-limitations-and-future-work)
- [Appendix A — Glossary](#appendix-a--glossary)
- [Appendix B — File map](#appendix-b--file-map)
- [Appendix C — Every bug found, in one table](#appendix-c--every-bug-found-in-one-table)

---

# Part I — The concepts, from zero

## 1. Vectors, embeddings, and nearest-neighbor search

Modern AI systems (like the retrieval step of a RAG — Retrieval-Augmented
Generation — pipeline) represent pieces of text, images, or documents as
**embeddings**: long arrays of floating-point numbers, e.g. 768 numbers per
document chunk. The key property is that *similar things get similar arrays*.

"Find documents relevant to this question" then becomes a geometry problem:

> Given a **query vector** q and a database of millions of **data vectors**,
> find the K vectors closest to q.

"Closest" is defined by a **distance metric**. This project supports:

- **L2 (Euclidean) distance**: `sum over i of (x[i] - y[i])²`. Smaller = more
  similar.
- **Cosine / inner-product family**: based on the dot product
  `sum of x[i] * y[i]`. SPTAG (see below) converts it into a
  "smaller-is-better" form: `base² − dot`, where `base` is 1 for float
  vectors and the max value of the integer type otherwise (e.g. 127 for
  int8). This looks odd but keeps one convention everywhere: **the smallest
  number wins**.

Finding the K nearest vectors is called **K-nearest-neighbor search (KNN)**,
and the returned set is the **top-K**.

## 2. Why brute force fails, and how SPANN fixes it

Brute force means: compute the distance from q to *every* database vector.
For 100 million vectors × 768 dimensions, that's ~77 billion multiplications
*per query*. Nobody does that at scale.

**SPTAG** (Space Partition Tree And Graph) is Microsoft's open-source
approximate nearest neighbor (ANN) library. The variant we use is **SPANN**,
designed for datasets too big for RAM. SPANN splits the data in two tiers:

1. **Head vectors** (a small sample, e.g. ~10% of the data) live in RAM
   inside an in-memory graph index (a "head graph": a BKT tree plus a
   relative neighborhood graph). Searching this graph quickly answers:
   *"which regions of space is q near?"* Each region is identified by a
   numeric **head ID**.

2. **Posting lists** live on cheap storage. A posting list is a blob of all
   the **tail vectors** (the other ~90%) assigned to one head — "everything
   in this region of space". It is keyed by the head ID.

A query then runs this pipeline:

```
                        ┌────────────────────────────────────────┐
   query vector q ────► │ 1. search head graph (in RAM, client)  │
                        │    → list of nearby head IDs           │
                        └───────────────┬────────────────────────┘
                                        │ head IDs
                        ┌───────────────▼────────────────────────┐
                        │ 2. fetch those posting lists           │
                        │    (from disk / KV store)              │
                        └───────────────┬────────────────────────┘
                                        │ blobs of tail vectors
                        ┌───────────────▼────────────────────────┐
                        │ 3. compute distance(q, tail) for every │
                        │    tail vector in the fetched postings │
                        └───────────────┬────────────────────────┘
                                        │ scored candidates
                        ┌───────────────▼────────────────────────┐
                        │ 4. merge into the global top-K answer  │
                        └────────────────────────────────────────┘
```

SPANN supports pluggable storage backends for step 2 (local files, RocksDB,
SPDK). **Before this project started, the SPTAG fork here had gained one
more backend: Aerospike** (`Storage=AEROSPIKEIO`), where each posting list is
one Aerospike record keyed by its head ID.

The crucial observation: in the stock design, steps 3 and 4 run **in the
client process**. Step 2 therefore ships *entire posting blobs* — megabytes
of raw vectors — over the network for every query, only for most of those
bytes to be reduced to a handful of (id, distance) pairs. That waste is what
this project attacks.

## 3. What Aerospike is

Aerospike is an open-source, distributed **key-value database** built for
low latency. Terms you need:

- **Record**: one key-value entry.
- **Namespace**: like a "database" — a storage policy domain (we use one
  named `test`, configured to store data in RAM).
- **Set**: like a "table" inside a namespace (we use `sptag`).
- **Bin**: like a "column" — a named field inside a record. Our posting
  blobs live in a single bytes bin named `value`.
- **Digest**: Aerospike does not route by your key directly. It hashes
  (RIPEMD-160) the set name + key into a 20-byte digest. The digest
  determines everything about placement.
- **Partition**: the keyspace is fixed at **4096 partitions**. The first
  bits of the digest pick the partition; the cluster assigns each partition
  to an owner node. Data is distributed by hashing, not by ranges.
- **Cluster**: nodes discover each other via a heartbeat protocol (we use
  "mesh" = explicit TCP seed addresses) and agree on the partition map.
- **Replication factor**: how many copies of each partition exist. We used
  1 (no replicas) for benchmarks to keep the math clean.

Aerospike's server is a C codebase. Requests arrive as binary messages over
TCP (see §7). One relevant fact about how the server executes work: a
request touching a record on the node that *owns* it is fast; a request for
a record the node doesn't own is normally *proxied* to the owner. The
EC528 `VECTOR_DISTANCE` operation (below) deliberately refuses to proxy —
it returns a per-key "wrong owner" status instead, pushing routing
responsibility to the client. That keeps the near-data property honest: work
happens only where the data physically lives.

## 4. What a "smart client" is

An Aerospike client library is a **smart client**: when it connects, it
downloads the cluster's partition map ("which node owns which of the 4096
partitions"). From then on, it computes each key's digest → partition →
owner **locally** and opens a direct TCP connection to the right node. No
central router, no extra hop.

For a batch of keys (like "score these 64 posting lists"), a smart client
does a **per-owner fanout**: it groups keys by owning node and sends each
node exactly one request carrying only that node's keys, in parallel. The
forked C client in this project implements exactly that for the
`VECTOR_DISTANCE` operation.

This matters for validation: when SPTAG asks for 64 heads spread over a
3-node cluster, three sub-requests fly in parallel and each node scores only
its own postings. Nothing needs to be forwarded between servers.

## 5. Near-data computing: the thesis of this project

"Near-data computing" (also "compute pushdown") means moving computation to
where data is stored instead of moving data to where the computation is.
The claim: for workloads that read a lot and return a little, the network
is the bottleneck, so pushing the reduction step server-side wins.

Our workload is the poster child. Per query, the baseline pipeline fetches
~64 posting lists. At dim 768 (float32 = 4 bytes/number), one tail vector is
3 KB; a posting list with ~100 tails is ~300 KB; 64 of them is ~19 MB *per
query* crossing the network — reduced client-side to just K=10 winners.

The offload pipeline sends each owner node the tiny query vector (3 KB) and
the list of head IDs, and gets back **owner-local top-K tuples**: about 18
bytes per candidate. The reduction happens on the node.

Measured in this run's full benchmark (1000 queries): baseline moved
**14.06 GB** between containers; offload moved **13.7 MB**. That ~1000×
reduction is not an implementation detail — it *is* the thesis.

The second half of the thesis: once the server does the scoring, the scoring
loop becomes the server's hot path — so make it fast with SIMD.

## 6. What SIMD is

A normal ("scalar") CPU instruction operates on one value: one add, one
multiply. **SIMD** (Single Instruction, Multiple Data) instructions operate
on a short array — a **vector register** — at once. A 128-bit register holds
4 float32 values ("4 lanes"); one SIMD multiply does 4 multiplications in one
instruction. Wider registers = more lanes:

| Instruction set | Register width | float32 lanes | Where it exists |
|---|---|---|---|
| NEON (ASIMD)    | 128-bit | 4  | Every 64-bit ARM CPU (Apple Silicon, Graviton…) — **always present**, no detection needed |
| SSE / SSE4.1    | 128-bit | 4  | Every x86-64 CPU (SSE4.1 since ~2008) |
| AVX2            | 256-bit | 8  | Most x86 CPUs since 2013 — **must be detected at runtime** |
| AVX-512         | 512-bit | 16 | Some Intel server CPUs — must be detected; also needs OS support |

Two subtleties that shaped this project:

**a) Runtime detection.** A program compiled with AVX2 instructions crashes
with "illegal instruction" on a CPU without AVX2. The standard pattern —
which we implemented — is: compile each instruction set into its **own
translation unit** (source file) with per-file compiler flags, keep the rest
of the program at a conservative baseline, and pick which function to call
**at runtime** using the `CPUID` instruction (x86's "what can this CPU do?"
query). There's a trap inside the trap: for AVX and AVX-512, the CPU
supporting the instructions is not enough — the *operating system* must also
save/restore the wider registers on context switches. That is checked with
the `XGETBV` instruction reading the XCR0 register. (SPTAG's own detection
code skips the XGETBV check; ours does it correctly.)

**b) Floating-point addition is not associative.** `(a + b) + c` and
`a + (b + c)` can round differently in float32. A scalar loop sums distances
sequentially; a SIMD loop keeps 4/8/16 partial sums (one per lane) and adds
them at the end — a *different grouping*, hence (tiny) different results.
This is unavoidable and industry-standard; it means SIMD results must be
compared to scalar results with a **tolerance**, not bit-for-bit. §15
explains exactly how big that tolerance must be and the surprising case
(int16 cosine) where it gets big enough to notice.

## 7. A crash course in wire protocols

A **wire protocol** is the byte-level format two programs use over a TCP
socket. Aerospike's:

- Every message starts with an 8-byte header: 1 byte version (2), 1 byte
  type (1 = "info" text protocol, 3 = binary message), 6 bytes of length.
- A binary message (`as_msg`) has a fixed 22-byte header (flags like
  "this is a write", result code, counts) followed by **fields** (typed,
  length-prefixed blobs — e.g. field type 0 = namespace name, 1 = set name,
  4 = digest) and **ops** (per-bin operations — e.g. op 2 = write a bin).

The EC528 phases (before this run) extended this protocol with two new field
types — and *only* fields; no new message kinds:

- **Field 44, `VECTOR_DISTANCE` request**: a little-endian packed struct:
  wire version (1), top-K, bin name, set name, the raw query vector bytes,
  and the list of int64 head IDs to score.
- **Field 45, `VECTOR_DISTANCE` response**: request status, then an array of
  scored results `(head_id int64, vid int32, version u8, distance float32)`
  — 18 bytes each — plus an array of per-key statuses for keys that could
  not be scored (missing, malformed, wrong owner…).

A reference implementation of this codec exists in pure Python
(`tests/conformance/vector_distance/codec.py`), which lets tests speak the
protocol with zero dependencies — that became the backbone of the live smoke
test (§19).

---

# Part II — The system as it existed before this run

## 8. The three repositories

All three are forks under `github.com/dzokha-true`, checked out side by side:

1. **`SPTAG-upstream`** (fork of `microsoft/SPTAG`, branch
   `feature/aerospike-compute`) — the ANN library. Before this run it had:
   the Aerospike KV backend (`Storage=AEROSPIKEIO`, posting lists stored as
   Aerospike records), and an *uncommitted, unverified* draft of the
   "vector distance offload" client hook (~800 lines: an env/INI-gated code
   path that calls the server instead of computing distances locally).
   The first thing this run did was preserve that draft as commit `1b4f9b2`.

2. **`aerospike-server-upstream`** (fork of `aerospike/aerospike-server`,
   branch `ec528/phase-3-vector-distance`) — the database. Before this run
   it had the complete **Phase 3** server operation: parsing field 44,
   validating namespace vector config, loading each requested record,
   walking the posting blob, computing distances (**scalar C++ code only**),
   keeping an **owner-local top-K** heap, and encoding field 45. It had unit
   tests (24 passing gtests) and an offline Python codec test. Its ADR
   (architecture decision record) 0004 explicitly named the follow-up this
   run delivered: *"optional SIMD in separate translation units with runtime
   CPUID dispatch."*

3. **`aerospike-client-c`** (fork of the official C client, branch `master`)
   — the client library. It had `aerospike_vector_distance()`: builds field
   44, performs the per-owner fanout (§4), reassembles results. Committed
   and pushed — but, as we discovered (§16), the repo **could not actually
   build** due to a Makefile bug, so none of it had ever run.

A fourth directory, `architect-loop`, contains the process tooling used to
run this project (Part IV) — it has nothing to do with vector search.

## 9. The architecture contract (who does what)

Both sides had ADRs freezing this division of labor, and this run did not
change it:

```
   CLIENT SIDE (SPTAG process)              SERVER SIDE (each Aerospike node)
 ┌──────────────────────────────┐         ┌──────────────────────────────────┐
 │ head graph search (RAM)      │         │ owns a subset of posting records │
 │ chooses head IDs             │  --->   │ VECTOR_DISTANCE:                 │
 │ per-owner fanout (C client)  │ field44 │  for each LOCAL requested head:  │
 │                              │         │   parse posting blob             │
 │ global merge of the          │ <---    │   distance(query, every tail)    │
 │ owner-local top-K lists      │ field45 │   keep owner-local top-K         │
 │ version/delete filtering     │         │  never proxies wrong-owner keys  │
 └──────────────────────────────┘         └──────────────────────────────────┘
```

Explicit non-goals, then and now: the ANN graph never moves to the server;
there is no server-to-server forwarding; there is no quantizer/compression
support in the operation; the client must fail loudly (never silently fall
back to the old path) if offload is enabled but unavailable.

Why this boundary? The graph is a complex, latency-sensitive, RAM-resident
structure — moving it into a database server would be a rewrite of SPTAG, not
an integration. Posting scoring, by contrast, is a pure function over bytes
the server already holds: perfect pushdown material.

## 10. The data formats, exactly

**A posting blob** (the value bin of one Aerospike record) is a sequence of
fixed-size elements, with **stride = 5 + dim × sizeof(value_type)**:

```
 ┌──────────┬─────────┬──────────────────────────────┐
 │ vid      │ version │ vector payload               │   × N elements,
 │ int32 LE │ uint8   │ dim × sizeof(type) bytes     │   back to back
 └──────────┴─────────┴──────────────────────────────┘
```

- `vid` is the tail vector's global ID (what search results return).
- `version` is SPTAG's freshness byte: SPTAG keeps a client-side "version
  map"; results whose version doesn't match are discarded by the client
  (the server just echoes the byte — it does not interpret SPTAG state).
- A blob whose size is not a multiple of the stride, or an element with a
  negative vid, is **malformed** → the server reports per-key status 5
  instead of results for that key.

**Namespace configuration**: the server refuses vector requests unless the
namespace declares `vector-dimension`, `vector-value-type`
(float/uint8/int8/int16) and `vector-metric` (l2/cosine/inner-product),
plus optional request-size limits. The query's byte length must equal
`dimension × sizeof(value_type)` exactly.

## 11. The distance math, exactly

The server's scalar kernels are a vendored, MIT-licensed adaptation of
SPTAG's own `DistanceUtils` — deliberately, so client-computed and
server-computed distances agree:

- L2: walk the arrays, `d = float(x[i]) − float(y[i])`, accumulate `d*d`
  into a **float32** accumulator (yes, even for integer input types — this
  detail matters in §15).
- Cosine family: accumulate `float(x[i]) * float(y[i])` (the dot product),
  then return `base² − dot`. Note cosine and inner-product **share one
  kernel** — SPTAG's convention; the server mirrors it. Distances can be
  legitimately negative; negative ≠ error.

---

# Part III — What this run built, issue by issue

The run was decomposed into 13 tracked issues (1 tracking + 12 work items),
each with acceptance checks frozen in git *before* implementation. Numbers
below are the issue numbers in
`aerospike-server-upstream/docs/issues/simd-vector-search/`.

## 12. Issue #2: the kernel dispatch table

**Problem**: Phase 3 hardcoded calls to scalar template functions. To add
SIMD, the server needs a way to (a) hold multiple implementations, (b) pick
one safely, (c) let operators control the choice.

**What was built** (`as/include/vector/vector_kernel.h`,
`as/src/vector/vector_kernel.c` — plain C, dependency-free):

- A **kernel table**: a 3-D lookup `rows[ISA][value_type][metric_family] →
  function pointer`, where ISA ∈ {SCALAR, SSE, AVX2, AVX512, NEON}, value
  type ∈ {float, uint8, int8, int16}, and metric family ∈ {L2, COSINE}
  (cosine and inner-product share the COSINE family, matching §11).
- The SCALAR row is **statically registered** (it always exists). SIMD rows
  register themselves from **constructor functions** — a C compiler feature
  (`__attribute__((constructor))`) that runs a function before `main()`.
  Each SIMD source file registers its own row, so adding an ISA never
  touches shared code.
- Each registration may carry a **runtime-support predicate** (e.g. "does
  this CPU have AVX2?"); a row is *available* only if registered in this
  build *and* its predicate passes on this machine.
- **Selection** reads the environment variable `AEROSPIKE_VECTOR_SIMD`:
  - `auto` (default): the best available row (highest enum value — safe
    because ARM rows and x86 rows can never coexist in one binary).
  - `scalar` / `sse` / `avx2` / `avx512` / `neon`: exactly that row.
  - Anything else, or naming a row that isn't available: a **fatal error**.

**Decision — crash, don't fall back.** If an operator writes
`AEROSPIKE_VECTOR_SIMD=avx2` on a machine without AVX2, the server calls
`cf_crash` on the first vector request. Rationale: a benchmark that
silently ran scalar while you believed it ran AVX2 produces *wrong
conclusions that look right* — the project's spec bans silent fallback
everywhere for this reason. The chosen ISA is also logged once
(`VECTOR_DISTANCE kernel isa 'neon' …`), and that log line later became the
proof, in live tests, of which code path really executed.

**Decision — the public compute function stays scalar.** The pre-existing
`as_vector_distance_compute()` now routes through the table, but **pinned to
the SCALAR row** forever. Why: every parity test needs a stable oracle
("the trusted reference answer"). If the oracle itself switched to NEON on
ARM, tests would be comparing NEON to NEON — circular. Hot-path code
resolves the *active* kernel separately.

**Decision — hoist the resolution.** Phase 3's scan loop re-decided
"which distance function?" for every tail vector — a `switch` on type and
metric per element, millions of times per second. The batch handler now
resolves the kernel **once per request** (a thread-safe `pthread_once` for
the env parsing plus one table lookup) and calls a bare function pointer in
the loop.

**Verification**: the gtest suite grew 24 → 31 (selection semantics: auto,
explicit, unknown string, unavailable ISA, error paths don't write output,
inner-product aliases cosine, the compute API's old failure contract
preserved).

## 13. Issue #3: NEON kernels for ARM

The development machine is Apple Silicon (arm64), and the benchmark cluster
runs arm64 Linux containers — so ARM SIMD is what actually gets measured
locally. NEON is baseline on every 64-bit ARM CPU: **no runtime detection
needed**, the row registers unconditionally on arm64 builds
(`as/src/vector/vector_distance_neon.cc`, guarded by `#if
defined(__aarch64__)`).

Design decisions, each with a reason:

- **Float-lane accumulation for every type.** The kernels widen inputs to
  exact integer forms where lossless (e.g. int8 diffs into int16, their
  squares into int32 — all exact), then convert to float32 and accumulate in
  4 float lanes. Why not accumulate integers all the way? (a) int16 squares
  can overflow int32; (b) the scalar oracle accumulates in float32, so a
  *more exact* integer sum would actually *diverge more* from the oracle
  under rounding. Matching the oracle's arithmetic domain minimizes the
  tolerance needed.
- **Loop widths by element size**: 16 elements/iteration for 8-bit types,
  8 for int16, 4 for float — each matching one full 128-bit register load.
- **No overreads.** A 16-wide loop only runs while ≥16 elements remain;
  leftovers use a scalar tail loop identical to the oracle's. This matters
  because posting payloads sit inside larger buffers — reading 16 bytes when
  7 remain could touch unmapped memory.
- One horizontal reduction (`vaddvq_f32`) at the end, then the scalar tail
  adds into the same running sum.

**Verification**: 5 new gtests — parity vs the scalar oracle for all 4 types
× 3 metrics × dims {1,3,4,7,8,15,16,31,32,33,64,100,128,768} (chosen to hit
every remainder-lane case), plus proof that `auto` selects NEON on arm64 and
explicit `scalar` still forces the oracle. Suite: 36/36.

## 14. Issue #4: SSE/AVX2/AVX-512 kernels + CPUID for x86

Three more translation units mirror the NEON design at 128/256/512-bit
widths (`vector_distance_{sse,avx2,avx512}.cc`), plus a new detection module
(`vector_cpu.c`).

Decisions and their reasons:

- **Per-file compiler flags, never global.** The server's global x86
  baseline is `-march=nocona` (a 2004-era CPU!) — deliberately conservative
  so one binary runs anywhere. Only the AVX2 file gets `-mavx2`, only the
  AVX-512 file gets `-mavx512f -mavx512bw`, wired for both the production
  Makefile objects and the test-binary objects. The baseline stays
  untouched; runtime dispatch does the rest.
- **Correct enablement checks.** `vector_cpu.c` checks CPUID leaf 1
  (SSE4.1, OSXSAVE, AVX), executes `XGETBV` (emitted as raw bytes
  `0f 01 d0` so the baseline-compiled file doesn't itself need special
  flags) to verify the OS saves ymm state for AVX2 — and additionally
  opmask/zmm state for AVX-512 — then CPUID leaf 7 (AVX2, AVX512F,
  AVX512BW). SPTAG's own `InstructionUtils` omits the XGETBV step; that
  omission was explicitly not copied.
- **The "SSE" row actually requires SSE4.1** (universal since ~2008),
  because the 8-bit kernels use `pmovzx/pmovsx` widening instructions.
  Recorded as a ruling; an explicit `sse` request on a pre-2008 CPU fails
  loudly like any unavailable ISA.
- **AVX-512 uses `madd` and cvt patterns gated on AVX512BW** — the integer
  instructions at 512-bit width live in the BW extension, so the predicate
  requires F+BW together.
- **Fresh code, not verbatim vendoring.** The original plan said "vendor
  SPTAG's x86 kernels." During implementation this was overruled (ruling
  recorded in the run's rulings file): SPTAG's int kernels accumulate in
  integer domains with per-ISA structural quirks, which diverges from the
  scalar oracle *more* than the float-lane design, and would have tripled
  the vendored surface. The shipped files keep the Microsoft MIT attribution
  header because their loop structure and distance semantics still derive
  from SPTAG's DistanceUtils.

**Verification on an ARM laptop — the honest version.** You cannot run x86
instructions on Apple Silicon natively. The harness
`tools/vector-x86-test.sh` runs the whole gtest suite inside an **emulated
x86_64 Linux container**. Reality found there: the emulator exposes SSE4.1
but **not AVX2** (verified by reading `/proc/cpuinfo` inside the container —
so our detection code was truthful, not buggy). Consequence, recorded as a
ruling rather than papered over: SSE parity is *proven* under emulation
(33 passed / 3 skipped, `X86_TESTS_OK`); AVX2/AVX-512 parity is *runtime-
gated but未proven on real hardware* (the fork's GitHub CI turned out to be
skip-gated on missing upstream secrets). This is listed as a residual risk
(§Part VI) — the failure mode is graceful (auto falls back to SSE), but a
native x86 run remains to-do.

## 15. Issue #5: the parity suite and the floating-point tolerance story

A dedicated randomized test (`vector_parity_test.cc`) compares **every
available ISA row** against the scalar oracle for **every dim from 1 to 67**
plus {100, 128, 768, 1024}, all types × metrics, with a logged seed.

It immediately caught something real. int16 cosine at dim 128 failed a
plain "within 0.001%" comparison. Was NEON wrong? No — and the analysis is
worth understanding:

- Every *individual* multiply matches bit-for-bit between scalar and NEON
  (the products are exact integers ≤ 2³⁰ that round identically).
- But full-range int16 dot products have terms up to ±1.07 × 10⁹ that
  largely **cancel**: the true sum (−1.37 × 10⁸ in the failing case) is 10×
  smaller than individual terms. When you add large cancelling values in
  float32, *the order of additions* changes which low bits survive — and
  scalar (sequential) vs SIMD (4 partial sums) order differs by design (§6b).
  The error isn't in either implementation; **the oracle itself is
  order-sensitive here**.
- The bound on that error is textbook numerical analysis: about
  `n × max_term × ε` where ε = 2⁻²⁴ for float32.

So the tolerance model became, documented in
`docs/benchmarks/simd-kernels.md` and encoded in all three parity tests:

```
tol = 1e-5 × max(1, |expected|, |actual|)   (relative part)
    + 4 × base(type)² × dim × 2⁻²³          (accumulation-order part)
```

The second term is negligible for float/int8/uint8 and only matters for
int16 under heavy cancellation — exactly matching observation.

The same issue also shipped a **microbenchmark** (`make -C as vector-bench`)
that times ns-per-vector for each ISA. Measured on Apple Silicon:

| type | metric | dim | scalar ns | NEON ns | speedup |
|---|---|---|---|---|---|
| float | l2 | 768 | 477.5 | 101.4 | **4.7×** |
| float | cosine | 768 | 324.9 | 95.4 | 3.4× |
| int8 | l2 | 768 | 608.2 | 70.9 | **8.6×** |
| int8 | cosine | 768 | 466.6 | 75.6 | 6.2× |

(int8 gains more because 16 elements fit one register vs 4 floats.)
Emulated x86 timings were deliberately **not** recorded — timing translated
instructions measures the emulator, not the kernels.

## 16. Issue #6: making the SPTAG build real (and porting SPTAG to ARM)

The SPTAG fork ships a Dockerfile so the whole ANN stack builds in a
container. It had two fatal problems and one hidden one.

**Problem 1 — the wrong client.** The Dockerfile installed the *stock*
Aerospike C client 7.3.0 from aerospike.com. The stock client has no
`aerospike_vector_distance.h` — so SPTAG's CMake probe for the offload API
could never succeed (the offload code would silently compile out!), and the
tarball was x86-only anyway. Fix: build the **forked** client from source
inside the image, verify the header landed, and make the image build **fail
loudly** if the CMake probe fails — plus write a marker file
(`/app/build/offload_probe_ok`) only on success, so later tests can prove
the build is "offload-real" instead of assuming it.

**Problem 2 — the client fork never built. Anywhere. Ever.** The forked
client's build failed inside `modules/mod-lua` with *"COMMON doesn't contain
'Makefile'"*. Diagnosis: the fork's earlier "space-safe make paths" commit
(made because this workspace's path contains spaces, which GNU make can't
handle in prerequisites) changed the top-level Makefile to pass
`COMMON="modules/common"` — a **relative** path — into sub-make invocations
that `cd` elsewhere first, where the path no longer resolves. Every build
since that commit had died there; the fork's local `target/` directory held
only partial objects, which is why nobody had noticed. Fix (on a new pushed
branch **`ec528/modules-abs-path`**, which the Dockerfile clones by name):
make the three module paths absolute with `$(CURDIR)`. (Space-containing
checkouts remain unbuildable — that's a GNU make limitation; build in
containers.)

**Problem 3 — SPTAG had literally zero non-x86 support.** On arm64 the
build exploded: `g++: unrecognized command line option '-mavx2'`. Pulling
the thread revealed x86 assumptions everywhere:

- CMake force-fed `-mavx2 -msse … -mavx512dq` to the DistanceUtils library
  on every GNU build → made conditional on the target processor.
- `InstructionUtils.h` unconditionally included `cpuid.h`, `xmmintrin.h`,
  `immintrin.h` — and core headers (KDTree, BKTree, the neighborhood graph)
  consume `_mm_prefetch` (an x86 cache-hint intrinsic) through it → added a
  guard macro `SPTAG_ARCH_X86` and, on other architectures, a shim mapping
  `_mm_prefetch` to the portable `__builtin_prefetch`.
- The runtime kernel selectors (`DistanceCalcSelector`, `SumCalcSelector`)
  referenced SSE/AVX functions unconditionally → on non-x86 they now return
  the portable scalar kernels.
- The SSE/AVX kernel bodies themselves → wrapped in the arch guard.
- `Core/Common.h` included x86-only `<mm_malloc.h>` for aligned allocation →
  non-x86 gets `posix_memalign`-backed `_mm_malloc/_mm_free` shims.

None of this changes behavior on x86 — every guard keeps the old path there.
The payoff: the **entire SPANN stack (ssdserving, SPTAGTest, SPFresh) builds
and runs natively on arm64 Linux**, with scalar client-side distances. That
is also the *fairest possible baseline* for this project: the client does
honest (scalar) work, and the server-side SIMD is what we measure. (Caveat
honestly recorded: on x86 the baseline client would itself use AVX,
narrowing the throughput gap — though not the network reduction.)

**Verification**: image builds on arm64; probe marker exact; the offload
unit-test suite (`/app/run-offload-tests.sh`, Boost.Test) passes **with the
real forked client compiled in** — first 9/9, then 10/10 after issue #7.
One path bug found late: SPTAG's CMake links test binaries into
`<source>/Release/`, not `build/Test/` — the runner script was fixed
accordingly.

## 17. Issue #7: the quantizer guard (a real bug found by review)

A **quantizer** (like Product Quantization) is a compression scheme: instead
of storing a 768-float vector (3 KB), store a learned compact code (e.g. 96
bytes). SPTAG supports optional quantizers; when active, the "query vector"
buffer SPTAG carries is the *encoded* form, whose size is
`QuantizeSize()` — not `dim × sizeof(type)`.

The pre-existing offload draft had a latent bug found during this run's
discovery review: it computed the wire query length as
`dim × sizeof(ValueType)` while passing the possibly-quantized buffer. With
a quantizer active it would have sent a mis-sized (garbage) query. The
server's ADR explicitly excludes quantizers from the operation, so the right
V1 behavior is **refuse, don't mangle**:

- `VectorDistanceOffload::Run` takes a new `quantizerActive` flag (the
  caller passes `p_index->m_pQuantizer != nullptr` — the quantizer is only
  knowable at search time, not construction time, which is why the guard
  lives here) and fails fast **before any network call**.
- A new unit test proves the fake KV backend records **zero** calls when the
  guard trips, plus the ADR gained the consequence note.

Suite went 9 cases/33 assertions → 10/36, verified both in a dev container
and inside the rebuilt image.

## 18. Issue #8: a Docker image for the server (and the graveyard of latent bugs)

To run live tests and clusters, the server needed a from-source Linux image
(none existed): `docker/Dockerfile` (multi-stage: build asd in an Ubuntu
22.04 builder layer, copy the binary into a slim runtime layer), a
**config template** rendered from environment variables (`VECTOR_DIM`,
`VECTOR_TYPE`, `VECTOR_METRIC`, `REPLICATION_FACTOR`, `NODE_ID`,
`MESH_SEED`, and `AEROSPIKE_VECTOR_SIMD` passed straight through to the asd
process), compose files for 1-node and 3-node clusters, and a smoke script.

Building asd from this branch for the first time on Linux was an
archaeology dig, because **the full server had never been compiled with the
vector code anywhere** — macOS can't build the production server (Linux-only
APIs), and the fork's CI never ran a full build. Every layer of failure was
real, pre-existing, and fixed:

1. Missing build dependencies in a clean container: `cmake` (the abseil/s2
   geometry deps) and `gcc-11-plugin-dev` (on arm64, Aerospike compiles a
   custom **gcc plugin** enforcing its memory-ordering model (TSO); the
   plugin needs gcc's own headers).
2. **Three stray closing braces** in `as/src/base/cfg_tree_handlers.cc` that
   ended functions early and left their bodies orphaned at file scope —
   syntax errors on any compiler. (Pattern found and fixed mechanically: a
   column-0 `}` followed by a blank line and an indented statement.)
3. The root Makefile's `targetdirs` rule creates object directories from a
   hardcoded list — `obj/vector` was never added when the vector module was
   created, so every vector object failed with "can't open dependency file".
4. The vector C++ build rule inherited `-std=gnu99` (a *C* standard flag)
   from the C flags; Apple's clang tolerates that on C++ files, **gcc under
   `-Werror` does not**. Fixed with the same `filter-out` the geospatial
   module already used.
5. A missing `struct as_namespace_s;` forward declaration (gcc -Werror), and
   a `/*` accidentally embedded *inside* a comment (`base/*` written in
   prose — gcc's `-Werror=comment` flags nested comment openers).
6. At runtime: asd exits unless its Lua UDF system directory exists → the
   runtime image pre-creates `/opt/aerospike/usr/udf/lua`.

Result: `bash docker/smoke-up.sh` boots a single vector-configured node and
prints `SERVER_VECTOR_READY` after functionally verifying the rendered
config. Lesson recorded in `docs/solutions/`: *unit tests green says nothing
about the production build; land the Linux image build early.*

## 19. Issue #10: the live wire smoke test

Everything so far tested components. `smoke.py` +
`run-smoke.sh` (in `tests/conformance/vector_distance/`) tests the **real
server over the real wire**, using only the Python standard library:

- It **writes its own fixtures** by hand-building standard Aerospike write
  messages (namespace/set/digest fields + one blob-bin write op) — the
  digest computed by the same RIPEMD-160 reference as the codec. No client
  library needed. (4 posting records × 50 tails × dim 64, plus one
  deliberately corrupt blob.)
- It then issues `VECTOR_DISTANCE` requests via the existing codec and
  checks: distances match a local pure-Python float32 reference within
  1e-5; a missing key reports status 2 and a malformed blob status 5
  *without* polluting the results; top-K truncation returns exactly K.
- The whole case set runs twice — against a container with
  `AEROSPIKE_VECTOR_SIMD=scalar` and one with `=neon` — and the two result
  snapshots must agree (same result sets, same statuses, distances within
  tolerance). The server's one-line ISA log is captured as proof each mode
  really ran. Output: `SMOKE_OK`.

One live-only wrinkle surfaced: writes in the first second after a node
reports ready can fail with result 11 ("partition unavailable") while the
initial partition balance settles; the smoke writer got a bounded retry.

This is also where the **dispatch table + NEON kernels ran inside a real
server for the first time** — and produced wire-identical behavior to
scalar, which is the whole point.

## 20. Issue #9: end-to-end parity (and two great debugging stories)

**The gate for the whole project** (spec ID `SPEC-4-INTEG-001`): prove that
turning the offload **on** does not change the answers. The harness
(`SPTAG-upstream/tests/integration/offload/run.sh`):

1. Boots one vector-configured asd container.
2. In the SPTAG container: generates a seeded dataset (5000 × dim-64
   float32) and **builds a real SPANN index with `Storage=AEROSPIKEIO`** via
   `ssdserving` (SPTAG's index build/search driver, controlled by an INI
   file) — head selection, graph build, and posting construction, with the
   583 resulting posting records written into Aerospike.
3. Runs the identical 100-query search twice with SPTAG's per-run flag:
   `SPTAG_AS_VECTOR_DISTANCE=0` (baseline: fetch postings via batch reads,
   compute distances in the client) and `=1` (offload). Each leg writes its
   per-query results via ssdserving's `SearchResult` file (binary: int32
   query count, int32 K, then (vid,dist) pairs — the layout was verified in
   SPTAG's source before parsing it).
4. Compares per-query top-10 sets (allowing legitimate ties to swap) and
   demands the server-side kernel log line as proof the offload leg really
   used the server op.

**Result: `PARITY_OK overlap=1.0000`** — identical top-10 for all 100
queries, server confirming `kernel isa 'neon'`.

Getting there surfaced two deep bugs, each a small saga:

**Saga 1 — "Failed to connect."** The very first run couldn't even build
the index: the client reported `aerospike_connect` failure. The diagnosis
ladder (preserved in `docs/solutions/client-rejects-sha-build-version.md`):
raw TCP from the container worked; DNS worked; a hand-rolled info-protocol
handshake worked perfectly (the server answered `node`, `features`,
`peers`); so the failure was *inside* the client. A 25-line C probe linking
the client with debug logging enabled revealed the real message the default
error hides: **"Node BB93… version is invalid: d6f6b9969."** The client
validates the server's *build version string* and rejects nodes that don't
look like `X.Y.Z…`. Our from-source asd reported its version as a bare git
SHA, because the version script (`build/gen_version`) derives it from
`git describe --tags` and **forks cloned without upstream's release tags
have no tag to describe** (Aerospike tags releases on release branches, so
even fetching tags didn't put one on our branch's ancestry). Fix: a local
annotated tag `8.0.0.0-start` on the branch base → version becomes
`8.0.0.0-start-N-g<sha>` → client parses `8.0.0.0` happily. ⚠️ That tag is
**local only** — anyone re-cloning these forks must recreate it (see
Part V troubleshooting).

**Saga 2 — the baseline segfault.** With connectivity fixed, the *offload*
leg worked but the *baseline* leg crashed (exit 139). Under gdb the crash
was `pthread_mutex_lock(mutex=0x68)` — locking through a null object —
inside `ThreadPool::add` called from `SearchIndex`. Explanation: SPANN's
dynamic searcher (SPFresh lineage) can trigger **background posting merges
during searches** (`AsyncMergeInSearch`, default on) when postings are small
(ours averaged ~8.6 vectors, below the merge threshold of 10). Those merge
jobs go to thread pools that are **only created when update mode is on** —
read-only search never initializes them → null pool → crash. This is a
latent upstream-fork bug unrelated to our changes. Two fixes: a null-guard
in `MergeAsync` (skip with a debug log instead of crashing), and
`AsyncMergeInSearch=false` in all harness INIs — the legs must not mutate
postings anyway, or they wouldn't be comparable.

## 21. Issue #11: the 3-node benchmark

The finale (`SPTAG-upstream/tests/benchmark/offload/run.sh`): a 3-node
Aerospike cluster (mesh heartbeat, replication-factor 1) plus the SPTAG
client container, comparing three configurations over the identical seeded
index and queries:

1. **baseline** — offload off; client fetches posting blobs and computes.
2. **offload-scalar** — offload on; servers forced to scalar kernels.
3. **offload-neon** — offload on; servers on NEON kernels.

Because the namespace is in-memory, each *server configuration* phase
rebuilds the (identical, seeded) index: phase A (scalar servers) hosts legs
1–2, phase B (neon servers) hosts leg 3. Metrics per leg: wall time and
QPS; **network bytes** summed from each node's `/sys/class/net/eth0`
counters (delta across the leg); **CPU** sampled from `docker stats` every
2 s; and per-query top-10 overlap vs the baseline leg.

**Quick mode** (10k × dim 64, 200 queries — the CI-style gate) printed
`BENCH_OK` with overlap exactly 1.0000 on both legs and the network story
already visible: 156.1 MB (baseline) vs 1.1 MB (offload).

**Full mode** (100,000 × dim 768 float32, 1000 queries, K=10):

| leg | wall (1000 q) | QPS | Σ node network bytes | avg node CPU | overlap vs baseline |
|---|---|---|---|---|---|
| baseline | 7.02 s | 142.5 | **14,057.7 MB** | 19% | — |
| offload + scalar | 3.28 s | 304.9 | 13.7 MB | 42% | 0.9994 (worst query 0.60) |
| offload + NEON | **2.15 s** | **465.1** | **13.7 MB** | 40% | 0.9986 (worst query 0.90) |

How to read this:

- **Network, ~1000× less**: the baseline ships every candidate posting blob
  (dim-768 float vectors ≈ 3 KB each) to the client per query; the offload
  ships back only ~18-byte scored tuples. This is the near-data thesis made
  measurable, and it is *structural* — it transfers to any real cluster.
- **Throughput 2.1× (scalar offload) → 3.3× (NEON offload)**: eliminating
  the transfer already wins; SIMD then cuts the server-side scoring cost
  for another 1.5×.
- **CPU 19% → ~41% on the data nodes**: the compute visibly *moved onto*
  the servers — which is precisely what "pushing compute to the data" means.
  (Baseline's 19% is the cost of serving huge batch reads.)
- **Overlap dips below 1.0 at scale** (0.9994/0.9986, always ≥ the 0.99
  gate): at 100k × 768 with replica postings, near-boundary ties and float
  ordering shuffle an occasional 10th-place result. Quick mode is exact.
- Honest caveats (in the results doc): everything shares one physical
  machine (absolute QPS numbers don't transfer; the network ratio does);
  ssdserving's per-query latency table has too-coarse timer resolution on
  the fast legs, so wall/QPS is the trustworthy aggregate; the arm64
  baseline client computes scalar distances (on x86 it would use AVX,
  narrowing — not erasing — the throughput gap).

One more robustness lesson: cluster nodes on a loaded laptop occasionally
took *minutes* before opening their client port, breaking log-grep
readiness checks. The harness now probes **functionally** — "does the
service port accept a TCP connection?" — which is exactly the condition the
legs need, instead of pattern-matching log lines.

## 22. Issues #12/#13: documentation

Everything above got written down where future readers will look:

- Server ADR 0004 records the shipped SIMD design (table, env var,
  CPUID/XGETBV policy, tolerance model, measured speedups); the Phase-4
  hand-off doc is marked COMPLETE with evidence pointers; the conformance
  README documents the live smoke.
- SPTAG ADR 0002 records the V1-synchronous decision (one client call per
  query batch; the async plumbing exists but is deliberately unused until
  latency data justifies it), the quantizer refusal, and the verification
  results; the fork glossary gained the "kernel table" term; the KV-backend
  contract doc lists the verified build & harness entry points.
- `docs/solutions/` in both repos holds the three war stories (version
  validation, first-Linux-build inventory, ARM port + client Makefile bug)
  written specifically so the *next* person doesn't re-derive them.

---

# Part IV — How the work itself was organized

You ran this with an orchestration method ("architect"): the work was
**specified before it was built**. Concretely:

1. A **spec** was written and approved
   (`aerospike-server-upstream/docs/spec/aerospike-simd-vector-search.md`),
   with explicit goals, non-goals, and assumptions (each assumption
   auto-ruled after a timed prompt, recorded, and vetoable).
2. The spec was decomposed into **13 tracked issues** (markdown files under
   `docs/issues/simd-vector-search/`), each with boundaries and a **frozen
   acceptance check** — a file of runnable commands committed to git
   *before* implementation, so success criteria couldn't drift to match
   whatever got built.
3. Work landed on a dedicated branch **`factory/simd-vector-search`** in
   both repos (pushed to your forks), one commit per issue, each commit
   message carrying its evidence. Issues were closed with verdicts and
   verbatim outputs in their comment logs.
4. Two research passes (run on the model you pinned for discovery) produced
   file:line-cited maps of both codebases before planning; they're archived
   under `docs/runs/simd-vector-search/` alongside the run manifest and the
   final digest.
5. Mid-run you redirected from "dispatch coding agents" to "implement
   directly" — that mode change is recorded in the rulings files, and the
   frozen checks stayed as the acceptance gates either way.

Deviations from plan were never silent: each one (fresh x86 kernels instead
of verbatim vendoring; AVX2 unprovable under emulation; scope additions like
the ARM port and the base/-directory brace fixes) is written down as a
ruling with its reasoning.

---

# Part V — How to run everything yourself

Everything below assumes Docker Desktop is running. Repos live side by side
in this directory; both are on branch `factory/simd-vector-search`.

**Server unit tests + kernel microbench (native, ~3 min):**

```bash
cd aerospike-server-upstream
make -C as run-vector-tests          # 37 passed, 1 skipped (x86 suite on ARM)
make -C as vector-bench && ./target/Darwin-arm64/bin/vector_bench
```

**Emulated x86 test leg (~15 min; SSE parity runs, AVX2/512 skip):**

```bash
bash tools/vector-x86-test.sh        # ends with X86_TESTS_OK
```

**Server image + single-node smoke (~20 min first build):**

```bash
docker build -f docker/Dockerfile -t as-vector:check .
bash docker/smoke-up.sh              # SERVER_VECTOR_READY
```

**Live wire smoke, scalar vs NEON (~5 min with image cached):**

```bash
bash tests/conformance/vector_distance/run-smoke.sh    # SMOKE_OK
```

**SPTAG image (~40 min first build; needs network):**

```bash
cd ../SPTAG-upstream
docker build -t sptag-offload-verify:check .
docker run --rm sptag-offload-verify:check /app/run-offload-tests.sh   # 10/10
```

**End-to-end parity (SPEC-4-INTEG-001, ~5 min with images cached):**

```bash
bash tests/integration/offload/run.sh          # PARITY_OK overlap=1.0000
```

**Benchmark:**

```bash
bash tests/benchmark/offload/run.sh quick      # BENCH_OK (~10 min)
bash tests/benchmark/offload/run.sh full       # the Part III §21 numbers (~30 min)
```

**Reclaiming disk space:**

Everything the harnesses put on disk is disposable and reproducible — the
datasets are generated from fixed seeds, so deleting them loses nothing:

```bash
# Generated vector datasets + built indexes (~386 MB after a full bench run;
# recreated automatically the next time a harness runs):
rm -rf SPTAG-upstream/tests/integration/offload/work \
       SPTAG-upstream/tests/benchmark/offload/work-*

# The two Docker images this project builds (~1.8 GB total; deleting them
# costs ~1 hour of rebuild on the next harness run):
docker rmi sptag-offload-verify:check as-vector:check

# Docker's build cache (several GB after the repeated builds; shared with
# every other Docker project on the machine — pruning slows their next
# builds too):
docker builder prune
```

Do **not** delete `SPTAG-upstream/datasets/SPACEV1B/` — those files predate
this project and are not regenerated by anything here. Aerospike itself
stores nothing on disk in this setup (the namespace is in-memory and lives
only as long as its container).

**Troubleshooting the sharp edges:**

- *Client says "Failed to connect" but the server is fine* → the asd build
  version is probably a bare SHA. Recreate the local tag:
  `git tag -a 8.0.0.0-start -m anchor <branch-base>` in the server repo and
  rebuild the image. (The tag is not pushed.)
- *`docker build` of SPTAG can't find `ThirdParty/zstd/build/cmake`* → run
  `git submodule update --init ThirdParty/zstd` first (the build context is
  your working tree).
- *Cluster nodes "not ready"* → they can take minutes on a busy laptop; the
  harnesses probe the service port for up to 8 minutes. Don't run other
  heavy builds concurrently.
- *Paths with spaces*: this workspace's path contains spaces. Everything
  here handles that, but GNU make fundamentally cannot — which is why the
  client fork must be built in containers (or from a space-free checkout).

---

# Part VI — Honest limitations and future work

1. **AVX2/AVX-512 parity has not executed on real x86 silicon.** The code
   is written, compiled (in the emulated container), runtime-gated, and SSE
   — which shares the structure — passes. But the local emulator exposes no
   AVX2, and the fork's CI workflow skips without upstream secrets. Until
   someone runs `tools/vector-x86-test.sh` (or the native suite) on an x86
   machine, treat AVX2/512 as "engineered, not proven". `auto` degrades
   safely to SSE/scalar regardless.
2. **Benchmark absolutes are single-host numbers.** Three "nodes" shared one
   laptop's cores, memory bandwidth, and a virtual network. The ~1000×
   network reduction is structural and will transfer; the specific QPS and
   latency numbers will not. A cloud x86 cluster run (also exercising AVX2
   server-side and an AVX client baseline) is the natural next experiment.
3. **The offload client path is synchronous (V1).** One
   `aerospike_vector_distance` call per query batch (internally concurrent
   across owners). The async plumbing exists but is unused — revisit only
   with latency data.
4. **Quantized indexes are refused, not supported.** By design (ADR); a
   future protocol revision could add an encoded-query mode.
5. **The architecture boundary is unchanged**: no server-side graph
   traversal, no cross-node merge, no proxying. Those are research
   directions, not gaps.
6. **Merge state**: `factory/simd-vector-search` is pushed in both forks
   and ready to merge (server → `ec528/phase-3-vector-distance`, SPTAG →
   `feature/aerospike-compute`). The client fix branch
   `ec528/modules-abs-path` should be merged to the client's `master` —
   until then the SPTAG Dockerfile deliberately clones that branch by name.
7. **The local version tag** (`8.0.0.0-start`) and the run's local commits
   exist only in these checkouts unless pushed; fresh clones must re-create
   the tag (Part V).

---

# Appendix A — Glossary

| Term | Meaning |
|---|---|
| ANN | Approximate nearest neighbor search — trades tiny accuracy for huge speed. |
| Bin | Aerospike record field (like a column). |
| Digest | RIPEMD-160 hash of set+key; determines a record's partition. |
| Head / tail vector | SPANN's split: heads live in the RAM graph; tails live in posting lists. |
| Head ID | Numeric ID of a head; also the Aerospike record key of its posting list. |
| ISA | Instruction set architecture row in the kernel table (scalar/sse/avx2/avx512/neon). |
| Kernel | One distance function for one (ISA, type, metric-family) combination. |
| Kernel table | The server's runtime dispatch structure over kernels (issue #2). |
| Lane | One element slot inside a SIMD register. |
| Namespace / set | Aerospike's database / table analogues. |
| Owner-local top-K | The best K scored tails among the postings a node itself owns. |
| Partition | One of Aerospike's fixed 4096 hash shards of the keyspace. |
| Posting list | Blob of tail vectors assigned to one head, stored as one record. |
| Quantizer | Vector compression (e.g. PQ); refused by the offload path. |
| Smart client | Client that maps key→partition→node itself and fans out per owner. |
| SPANN | SPTAG's disk-scale index: RAM head graph + stored posting lists. |
| ssdserving | SPTAG's INI-driven index build/search driver binary. |
| Stride | Bytes per posting element: 5 + dim × sizeof(type). |
| VECTOR_DISTANCE | The EC528 server operation (wire fields 44/45): score listed postings, return owner-local top-K. |
| VID | A tail vector's global integer ID. |
| Wire protocol | The byte format exchanged over TCP. |
| XGETBV / XCR0 | x86 mechanism to check the OS enabled AVX/AVX-512 register state. |

# Appendix B — File map

**aerospike-server-upstream** (branch `factory/simd-vector-search`)

| Path | What it is |
|---|---|
| `as/include/vector/vector_kernel.h`, `as/src/vector/vector_kernel.c` | The kernel dispatch table (issue #2). |
| `as/src/vector/vector_distance_neon.cc` | NEON kernels (#3). |
| `as/src/vector/vector_distance_{sse,avx2,avx512}.cc`, `vector_cpu.c`, `as/include/vector/vector_cpu.h` | x86 kernels + CPUID/XGETBV detection (#4). |
| `as/src/vector/vector_{kernel,neon,x86,parity}_test.cc`, `vector_bench.cc` | Tests + microbench (#2–#5). |
| `as/src/vector/vector_batch.c` | The request handler; per-request kernel resolution + ISA log line. |
| `as/src/vector/vector_distance.cc`, `as/include/vector/sptag_distance.h` | The scalar oracle (pre-existing, now table-registered). |
| `docker/` | Server image, config template, compose files, smoke-up (#8). |
| `tests/conformance/vector_distance/{codec,smoke}.py`, `run-smoke.sh` | Wire codec + live smoke (#10). |
| `tools/vector-x86-test.sh` | Emulated x86 test leg (#4/#5). |
| `docs/spec/…`, `docs/issues/simd-vector-search/`, `docs/checks/…`, `docs/runs/…`, `docs/jobs/…` | The run's spec, tracker, frozen checks, manifest/digest, rulings. |
| `docs/adr/0004…`, `docs/benchmarks/simd-kernels.md`, `docs/solutions/…` | Design record, kernel results + tolerance model, war stories. |

**SPTAG-upstream** (branch `factory/simd-vector-search`)

| Path | What it is |
|---|---|
| `AnnService/inc/Core/SPANN/VectorDistanceOffload.h` | The offload client hook (pre-existing draft, now guarded + verified). |
| `AnnService/src/Helper/AerospikeKeyValueIO.cpp` | The Aerospike backend incl. `VectorDistance` call. |
| `AnnService/inc/Core/Common/{InstructionUtils,DistanceUtils,SIMDUtils}.*`, `inc/Core/Common.h` | The ARM port guards/shims (#6). |
| `AnnService/inc/Core/SPANN/ExtraDynamicSearcher.h` | Search pipeline; offload gate; MergeAsync null-guard. |
| `Dockerfile`, `Script_AE/run-offload-tests.sh` | The offload-real image (#6). |
| `Test/src/VectorDistanceOffloadTest.cpp` | Offload unit suite (10 cases). |
| `tests/integration/offload/` | End-to-end parity harness (#9). |
| `tests/benchmark/offload/`, `docs/benchmarks/phase-4-results.md` | Benchmark harness + results (#11). |
| `docs/adr/0002…`, `docs/contracts/aerospike-kv-backend.md`, `CONTEXT.md` | Design record, backend contract, glossary. |

**aerospike-client-c**: branch `ec528/modules-abs-path` = master + the
Makefile fix that makes the fork buildable (§16); `master` holds the
`aerospike_vector_distance` implementation itself (pre-existing).

# Appendix C — Every bug found, in one table

| # | Where | Bug | How found | Fix |
|---|---|---|---|---|
| 1 | SPTAG offload draft | Query length computed as raw size while buffer may be quantizer-encoded | Discovery review before coding | Fail-fast quantizer guard + test (#7) |
| 2 | client fork Makefile | Relative `COMMON=modules/common` passed into sub-makes → **fork never built at all** | First docker build of the client | `$(CURDIR)` absolute paths, branch `ec528/modules-abs-path` |
| 3 | SPTAG (all of it) | Zero non-x86 support: forced `-mavx2` flags, unconditional `cpuid.h`/`mm_malloc.h`, unguarded SIMD selectors | First arm64 build | `SPTAG_ARCH_X86` guards, prefetch/malloc shims, scalar selector fallback |
| 4 | server `cfg_tree_handlers.cc` | Three stray `}` orphaning function bodies (syntax errors) | First full Linux build of the branch | Mechanical brace scan + removal |
| 5 | server root Makefile | `targetdirs` never created `obj/vector` | Same build | Added the directory |
| 6 | server vector Makefile rule | `-std=gnu99` leaked into C++ compiles (fatal under gcc `-Werror`) | Same build | `filter-out`, as geospatial already did |
| 7 | server `vector_types.h` / kernel header | Missing struct forward decl; `/*` inside a comment | Same build | Trivial fixes |
| 8 | server runtime image | asd exits without the Lua UDF directory | First container boot | Pre-create dirs |
| 9 | client library behavior | Rejects servers whose build version isn't `X.Y.Z` — from-source forks report a git SHA | TCP fine → info fine → C probe with debug log | Local `8.0.0.0-start` tag anchors `gen_version` |
| 10 | SPANN dynamic searcher | Read-only search fires `MergeAsync` into thread pools that only exist in update mode → null-pointer segfault | gdb backtrace on the baseline leg | Null-guard + `AsyncMergeInSearch=false` in harness INIs |
| 11 | live cluster ops | Post-startup writes briefly return result 11; nodes can take minutes to open their port under load | Smoke/bench flakes | Bounded write retry; functional port-probe readiness |

---

*Written 2026-07-15 at the close of run `simd-vector-search`. The full audit
trail — spec, issue-by-issue evidence, frozen checks, rulings, and the final
digest — lives in `aerospike-server-upstream/docs/{spec,issues,checks,runs,jobs}/`.*
