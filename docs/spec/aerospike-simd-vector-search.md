# Spec: Aerospike-server-side SIMD-enabled closest vector search

Run: `simd-vector-search` · Date: 2026-07-14 · Status: **APPROVED**

Approval record: in-session, 2026-07-14, repo owner replied verbatim:
"implement them" — full-scope authorization for WS-A, WS-B, WS-C as specified.
Rulings: `docs/runs/simd-vector-search/rulings-pending.md`
Discovery evidence (luna-high, file:line-cited):
`docs/runs/simd-vector-search/discovery-server.md`, `discovery-sptag.md`

## Goal

Finish pushing SPANN tail-vector compute onto Aerospike nodes and make it fast:

1. **SIMD distance kernels in the Aerospike server** (`VECTOR_DISTANCE` op),
   x86 (SSE/AVX2/AVX-512, runtime CPUID dispatch) and arm64 NEON, replacing the
   shipped scalar-only kernels — the ADR 0004 named follow-up.
2. **Complete and verify the SPTAG Phase-4 offload** (WIP commit `1b4f9b2` on
   `feature/aerospike-compute`): the flag-gated path that replaces client-side
   `MultiGet + ComputeDistance` with server-side `VECTOR_DISTANCE`.
3. **Prove it**: kernel parity tests, end-to-end top-K parity vs the baseline
   path, live conformance smoke, and a baseline / offload-scalar / offload-SIMD
   benchmark with p50/p95/p99, QPS, per-node CPU, and network bytes.

## Non-goals

- No server-side head-graph traversal, cross-key/global merge, or graph storage
  in Aerospike (ADR 0002/0004 boundary stands; SPTAG stays the smart client).
- No wire-protocol changes (fields 44/45 v1 payload frozen).
- No quantizer/PQ/OPQ support in the offload path (guarded, see WS-B1).
- No upstreaming to Aerospike Inc. or Microsoft; no production hardening.

## Assumptions (auto-rulings, 5m silence; vetoable)

- A1: The uncommitted Phase-4 work is kept (now local commit `1b4f9b2`), not
  reimplemented. All three repos otherwise match origin; no resets performed.
- A2: SIMD targets both x86-64 (runtime dispatch above the `-march=nocona`
  baseline) and arm64 (NEON at baseline — armv8 always has NEON, so no runtime
  dispatch needed on ARM).
- A3: "Closest vector search" = the existing operator (score listed posting
  records, owner-local top-K), SIMD-accelerated. Not an architecture change.
- A4: Full validation scope: kernel parity + microbench, SPEC-4-INTEG-001
  end-to-end parity, live smoke, cluster benchmark (local Docker; x86 cloud
  benchmark is stretch).
- A5: Plan-only run; if a factory run is approved later, coordination home is
  `aerospike-server-upstream` (markdown mode). Codebase-discovery dispatches
  run on codex `gpt-5.6-luna` effort=high (canary SHELLS_OK, `.architect/config`).

## Current state (evidence-grounded)

| Repo / branch | State |
|---|---|
| `aerospike-server-upstream` · `ec528/phase-3-vector-distance` | `VECTOR_DISTANCE` shipped: wire (fields 44/45), posting parse, owner-local top-K, namespace config, gtests (`make -C as run-vector-tests`) + offline Python codec conformance. Distance kernels **scalar only** (`as/include/vector/sptag_distance.h`, `as/src/vector/vector_distance.cc`). No CPU-feature detection anywhere in the codebase. |
| `aerospike-client-c` · `master` | `aerospike_vector_distance.{h,c}` + async per-owner fanout committed & pushed. Believed feature-complete; verified in WS-B2. |
| `SPTAG-upstream` · `feature/aerospike-compute` | KV backend committed; Phase-4 offload preserved as WIP `1b4f9b2`: INI param `VectorDistanceOffload` + env `SPTAG_AS_VECTOR_DISTANCE`, `KeyValueIO::VectorDistance` virtual, `AerospikeKeyValueIO` impl (single concurrent batch call), fail-fast semantics, mock-based test suite (no live server needed). CMake probe defines `SPTAG_HAS_AEROSPIKE_VECTOR_DISTANCE`. |

Key discovery facts the plan builds on:

- Server kernel seam: `compute_typed<T>()` (`as/src/vector/vector_distance.cc:16-35`)
  is the single replacement point; hot path `as_vector_batch_handle()`
  (`as/src/vector/vector_batch.c:281-303`) resolves the kernel per posting
  element and should hoist resolution out of the scan loop.
- Build seam: add TUs to `VECTOR_SOURCES` (`as/src/Makefile:159-166`) with
  target-specific per-object `CXXFLAGS` (pattern at `:352-356`). x86 baseline
  is `-march=nocona`; arm64 baseline `-mcpu=neoverse-n1` (NEON included).
- Vendorable kernels: SPTAG `DistanceUtils.{h,cpp}` — real SSE/AVX2/AVX-512
  intrinsics for float/int8/uint8/int16, L2 + cosine-family (`base² − dot`,
  smaller-better), MIT headers present. `InstructionUtils` (CPUID) lacks a
  license header in the fork — take from upstream microsoft/SPTAG or write
  fresh. **No NEON exists in SPTAG**; ARM kernels are new code.
- Latent defect found: `VectorDistanceOffload::Run` sends
  `dim * sizeof(ValueType)` bytes but passes `GetQuantizedTarget()`, whose size
  is `IQuantizer::QuantizeSize()` — wrong query length if a quantizer is active
  (`VectorDistanceOffload.h:124-144`).

## Domain language

Per `SPTAG-upstream/CONTEXT.md` glossary: head graph, posting list, tail
vector, Owner-Local Top K, VECTOR_DISTANCE, vector distance offload. New term
introduced by this spec: **kernel table** — the per-`(value_type, metric-family,
ISA)` function-pointer table resolved once per process (x86) or fixed at
compile time (arm64).

## Workstreams and slices

### WS-A — Server SIMD (aerospike-server-upstream)

- **A1 · Dispatch-seam refactor (structural).** Introduce the kernel table
  behind `as_vector_distance_compute()`; resolve once, not per element (hoist
  in `as_vector_batch_handle()` scan loop). Scalar kernels only; behavior
  byte-identical. Add config/env override `vector-simd = auto|scalar|sse|avx2|
  avx512|neon` (invalid/unsupported → startup failure, no silent fallback) for
  A/B benchmarking. Checks: existing gtests green; new dispatch unit tests.
- **A2 · x86 SIMD TUs (behavioral, blocked by A1).** Vendor SPTAG
  `DistanceUtils` SSE/AVX2/AVX-512 kernels into
  `as/src/vector/vector_distance_{sse,avx2,avx512}.cc`, one TU per ISA with
  per-object flags; MIT headers preserved. New `vector_cpu.c` CPUID helper
  (leaf 1 + leaf 7 + XGETBV/OSXSAVE check — SPTAG's `InstructionUtils` omits
  the xcr0 check; do it correctly). Selection AVX-512 → AVX2 → SSE → scalar.
- **A3 · NEON TUs (behavioral, blocked by A1; parallel to A2).** New NEON
  kernels for float first, then int8/uint8/int16, compiled at the arm64
  baseline (no runtime detection needed). This is what benchmarks on the dev
  Mac and any arm64 deployment.
- **A4 · Kernel parity + microbench (blocked by A2/A3).** Randomized-vector
  gtests: every (type × metric × ISA) vs the scalar oracle, dims covering all
  remainder lanes (e.g. 1…67, 100, 768, 1024). Tolerance policy: integer paths
  exact; float paths relative tolerance (accumulation-order differences), and
  document it. Microbenchmark (ns/vector vs dim) recorded in
  `docs/benchmarks/simd-kernels.md`. x86 runs on CI (GitHub x86-64 runners
  cover SSE/AVX2; AVX-512 gated on runtime detection, validated
  opportunistically or on a cloud VM).

### WS-B — SPTAG offload completion (SPTAG-upstream; parallel to WS-A)

- **B1 · Quantizer guard (fix the found defect).** Refuse offload (fail fast at
  Available()/setup, matching ADR 0002 semantics) when a quantizer is active,
  or size the query from `QuantizeSize()`; ADR 0004 excludes quantizers, so
  guard + test is the recommended fix.
- **B2 · Real-build verification.** Build SPTAG `-DAEROSPIKE=ON` against the
  installed client-c fork so the CMake probe defines
  `SPTAG_HAS_AEROSPIKE_VECTOR_DISTANCE`; run `SPTAGTest` including
  `VectorDistanceOffloadTest`; confirm the client API/ABI actually matches
  (the mock suite never touches the real header). Decide and document whether
  the ignored async-request parameter stays synchronous in V1 (recommended:
  yes; note it in ADR 0002).
- **B3 · SPEC-4-INTEG-001 end-to-end parity (blocked by B2; server can be
  scalar).** Single-node Dockerized asd with vector config; build a small
  `Storage=AEROSPIKEIO` index; assert top-K parity (overlap threshold, tie
  ordering allowed to differ) between offload-on and baseline
  `MultiGet + ComputeDistance` over a query set. This is the Phase-4 gate.

### WS-C — Integration, benchmark, docs (blocked by WS-A + WS-B)

- **C1 · Live conformance smoke.** The planned `smoke.py` against a live asd
  (codec already supports raw-socket construction), run with SIMD off/on to
  prove wire-visible behavior is unchanged.
- **C2 · Cluster benchmark.** 3-node Docker cluster (arm64 locally):
  baseline vs offload-scalar vs offload-NEON; same-QPS p50/p95/p99, max QPS at
  p99 budget, per-node CPU, network bytes per query, top-K overlap. Dataset:
  SPACEV1B subset or synthetic at realistic dim (e.g. 768 float32). Results in
  `docs/benchmarks/phase-4-results.md`. Stretch: x86 cloud VMs for AVX2/AVX-512.
- **C3 · Docs debt.** Update ADR 0004 (follow-up delivered) + ADR 0002 (sync
  decision, quantizer guard), contract docs, `docs/solutions/` notes,
  benchmark methodology.

Dependency graph: A1 → {A2, A3} → A4; B1, B2 independent; B2 → B3;
{A4, B3} → C1 → C2 → C3. Max parallelism after A1: A2, A3, B1, B2.

## Validation strategy

Layered oracles, each independent of the code it checks: scalar kernels are
the SIMD oracle (A4); the baseline client path is the offload oracle (B3); the
offline codec suite plus live smoke are the wire oracle (C1); benchmarks are
measurement only, never correctness evidence. All new tests run in the
existing harnesses (`make -C as run-vector-tests`, `SPTAGTest`, `python3 -m
unittest`, docker compose for live runs).

## Risks

- `-Werror` + vendored intrinsics code and per-TU ISA flags: keep vendored
  code in isolated TUs; do not widen global flags.
- Float parity is not bit-exact across ISAs (horizontal-sum order): tolerance
  policy is part of A4's frozen check, not ad hoc.
- AVX-512 has no local execution target (Apple Silicon; Rosetta lacks AVX-512):
  correctness gated on runtime detection; benchmark claims limited to ISAs
  actually measured.
- Near-tie top-K reordering between scalar and SIMD: parity defined as set
  overlap with distance tolerance, tie order free.
- `InstructionUtils` license header missing in fork: vendor from upstream with
  headers intact or write fresh CPUID code (A2 does the latter).
- Client-c "believed complete" is unverified against a live server: B2/B3
  surface any ABI gaps early; a client fix would be a small follow-on issue in
  `aerospike-client-c`.

## Open human decisions

None blocking. Vetoable defaults: assumptions A1–A5 above.
