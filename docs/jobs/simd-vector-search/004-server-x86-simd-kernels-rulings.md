# Rulings: 004-server-x86-simd-kernels (append-only, orchestrator-owned)

- 2026-07-14 RULING (post-freeze, implementation shape): the x86 TUs are
  written fresh in the same structure as the NEON TU (uniform float-lane
  accumulation with exact integer widening/madd where lossless), rather than
  transplanting SPTAG DistanceUtils kernel bodies verbatim. Why: SPTAG's
  int kernels accumulate in integer domains with per-ISA structural
  differences, which diverges from the scalar float-accumulator oracle more
  than a float-lane design and triples the vendored surface. The TUs keep
  the Microsoft MIT attribution header (loop structure and distance
  semantics adapted from SPTAG DistanceUtils, same as sptag_distance.h).
  Frozen-check RUN items are unaffected (MIT grep, xgetbv grep, per-object
  flags, X86_TESTS_OK, native suite green); the judge-only item "vendored
  TUs contain no SPTAG typedefs" holds by construction.
- 2026-07-14 RULING (SSE row): the "sse" row requires SSE4.1 at runtime
  (universal since 2008) because 8-bit widening uses pmovzx/pmovsx. Recorded
  in vector_cpu.c; explicit "sse" on a pre-SSE4.1 CPU fails selection loudly
  like any unavailable ISA.
- 2026-07-14 RULING (post-freeze, environment): this host's x86 emulation
  (Docker Desktop) exposes sse4_1 but NOT avx2 (verified via /proc/cpuinfo
  inside the amd64 container - "no avx2 in cpuinfo"), so the frozen-check
  expectation "AVX2 parity ran under emulation" is unsatisfiable locally.
  Detection code is truthful (cpu: sse41=1 avx2=0 avx512bw=0). Compensating
  evidence: AVX2 parity runs natively on x86_64 GitHub CI after the factory
  branch push; AVX-512 stays runtime-gated opportunistic as specced. SSE
  parity ran and passed under emulation (X86_TESTS_OK, 33 passed/3 skipped).
