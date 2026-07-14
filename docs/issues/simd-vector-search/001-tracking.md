---
issue: 1
title: "Tracking: Aerospike-server-side SIMD-enabled closest vector search"
state: OPEN
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
