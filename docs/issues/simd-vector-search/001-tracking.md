---
issue: 1
title: "Tracking: Aerospike-server-side SIMD-enabled closest vector search"
state: CLOSED
parent: none
blocked-by: none
---
Tracking issue for run `simd-vector-search`.

Spec: `docs/spec/aerospike-simd-vector-search.md` (approved in-session
2026-07-14, verbatim: "implement them").
Manifest: `docs/runs/simd-vector-search/manifest.md`.

Assumptions digest (auto-rulings, vetoable): keep WIP `1b4f9b2` as the SPTAG
starting point; SIMD targets x86 (SSE/AVX2/AVX-512, runtime CPUID dispatch)
and arm64 NEON (baseline); ADR-0004 operator boundary unchanged; full
validation scope (kernel parity, SPEC-4-INTEG-001, live smoke, cluster
benchmark); coordination home is this repo in markdown mode.

Approve-by-comment: the repo owner may comment `APPROVE`, `APPROVE with
edits: <text>`, or `REJECT <reason>` here at any time; the run is already
authorized in-session.

Sub-issue lanes:
- Server lane (this repo): #2 kernel dispatch -> #3 NEON -> #4 x86 SIMD ->
  #5 parity+bench; #8 Linux image (after #2); #10 live smoke; #12 docs.
- SPTAG lane (SPTAG-upstream): #6 docker build verify -> #7 quantizer guard ->
  #9 integration parity; #11 benchmark; #13 docs.

<!-- architect-run: simd-vector-search -->

## Comments
- 2026-07-14T04:30Z [orchestrator] Run opened. Backend canaries: codex CLI SHELLS_OK (gpt-5.6-luna researchers pin; builders codex/best gpt-5.5 xhigh per ~/.architect/config). Baseline evidence: server vector gtests 24/24 PASS on Darwin arm64; SPTAG WIP preserved at 1b4f9b2; docker daemon linux/arm64 up; server submodules init started.
- 2026-07-15T00:05Z [orchestrator] DIGEST: run complete; all 12 sub-issues closed PASS. Shipped: (server) kernel dispatch table + AEROSPIKE_VECTOR_SIMD, NEON kernels (3.4-4.7x float / 5.5-9.1x int8 vs scalar), x86 SSE/AVX2/AVX-512 kernels + correct CPUID/XGETBV gating, randomized parity suite with documented accumulation-order tolerance, kernel microbench, Linux asd docker image + single/3-node compose, live wire smoke (SMOKE_OK, scalar-vs-neon equivalence); (SPTAG) real offload docker build on the forked client, full arm64 port of SPTAG's x86-only code, quantizer fail-fast guard, MergeAsync null-guard, SPEC-4-INTEG-001 parity harness (PARITY_OK 1.0000), 3-node benchmark (baseline 142.5 QPS/14.06 GB net vs offload-neon 465.1 QPS/13.7 MB, overlap 0.9986-0.9994); (client) modules-abs-path build fix branch pushed (fork never built since the space-safe commit). Pre-existing breakage fixed en route: cfg_tree_handlers stray braces (x3), targetdirs missing obj/vector, -std=gnu99 in C++ compiles, missing forward decl, client rejecting SHA build versions (local 8.0.0.0-start tag anchors gen_version). Residual risks: AVX2/AVX-512 parity not run on native x86 hardware (emulator lacks avx2; fork CI is secrets-gated; kernels are runtime-gated so unsupported hosts fall to SSE/scalar via auto); benchmark numbers are single-host docker (network reduction is structural; absolute QPS is not); occasional multi-minute cluster startup stalls on this host (harness gates on a port probe). Skipped/stretch: cloud x86 end-to-end benchmark. Merge instructions: factory/simd-vector-search in BOTH repos is ready; merge server branch into ec528/phase-3-vector-distance (or keep as the new line), SPTAG branch into feature/aerospike-compute; aerospike-client-c branch ec528/modules-abs-path should merge to master (the Dockerfile clones it by name).
