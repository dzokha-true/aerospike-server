# SIMD kernel parity and microbenchmarks (run simd-vector-search)

Date: 2026-07-14. Host: Apple Silicon (Darwin arm64), Docker Desktop for the
x86_64 emulated leg.

## Tolerance policy (parity vs the scalar oracle)

`as_vector_distance_compute()` is pinned to the scalar kernel row and serves
as the oracle. SIMD rows are compared with:

```
tol = 1e-5 * max(1, |expected|, |actual|)      (relative, floor at 1e-5)
    + 4 * base(type)^2 * dim * 2^-23           (accumulation-order bound)
```

Rationale: per-element terms are mathematically identical between scalar and
SIMD (integer widening/madd is exact; float multiplies round identically),
but the scalar loop sums sequentially while SIMD sums per lane and reduces
once. Float addition is not associative, so under heavy cancellation (signed
dot products at full int16 range) the order-dependent error grows as
`n * max_term * epsilon`. The second term bounds that; for float/uint8/int8
it is negligible, for int16 it dominates only in cancellation-heavy cases.
Empirically (seed 20260714, dims 1..67 and {100,128,768,1024}, all types x
all metrics): NEON and SSE pass everywhere with this bound; the pure 1e-5
relative bound fails only for int16 cosine/inner-product at dim >= 128 under
full-range cancellation — in both directions, i.e. the oracle itself is
order-sensitive there.

## Native arm64: scalar vs NEON (ns/vector; higher speedup is better)

`make -C as vector-bench && ./target/Darwin-arm64/bin/vector_bench`
(Apple Silicon, macOS; -O2; pool of 256 tail vectors)

| type | metric | dim | isa | ns/vector | speedup |
|---|---|---|---|---|---|
| float | l2 | 128 | scalar | 60.4 | 1.00x |
| float | l2 | 128 | neon | 13.2 | 4.58x |
| float | cosine | 128 | scalar | 34.3 | 1.00x |
| float | cosine | 128 | neon | 9.6 | 3.57x |
| float | l2 | 768 | scalar | 477.5 | 1.00x |
| float | l2 | 768 | neon | 101.4 | 4.71x |
| float | cosine | 768 | scalar | 324.9 | 1.00x |
| float | cosine | 768 | neon | 95.4 | 3.40x |
| float | l2 | 1024 | scalar | 674.4 | 1.00x |
| float | l2 | 1024 | neon | 155.0 | 4.35x |
| float | cosine | 1024 | scalar | 463.4 | 1.00x |
| float | cosine | 1024 | neon | 134.8 | 3.44x |
| int8 | l2 | 128 | scalar | 72.1 | 1.00x |
| int8 | l2 | 128 | neon | 8.0 | 9.06x |
| int8 | cosine | 128 | scalar | 61.1 | 1.00x |
| int8 | cosine | 128 | neon | 9.3 | 6.60x |
| int8 | l2 | 768 | scalar | 608.2 | 1.00x |
| int8 | l2 | 768 | neon | 70.9 | 8.58x |
| int8 | cosine | 768 | scalar | 466.6 | 1.00x |
| int8 | cosine | 768 | neon | 75.6 | 6.17x |
| int8 | l2 | 1024 | scalar | 832.0 | 1.00x |
| int8 | l2 | 1024 | neon | 109.6 | 7.59x |
| int8 | cosine | 1024 | scalar | 638.3 | 1.00x |
| int8 | cosine | 1024 | neon | 115.3 | 5.54x |

## EMULATED x86_64 leg (correctness only - NOT a performance claim)

`bash tools/vector-x86-test.sh` runs the full gtest suite (including the
randomized parity suite) in an x86_64 Linux container on this arm64 host.
The emulation layer (Docker Desktop / Rosetta) exposes SSE4.1 but not AVX2
or AVX-512 here (verified via /proc/cpuinfo), so:

- SSE parity RUNS and passes under emulation.
- AVX2 parity SKIPS locally; it runs natively on x86_64 GitHub CI
  (workflow_dispatch on the factory branch), whose runners expose AVX2.
- AVX-512 parity is runtime-gated and validated opportunistically on
  hardware that has it; the CPUID gate (XCR0 opmask/zmm + AVX512F+BW) keeps
  it off everywhere else.

Emulated timing numbers are meaningless (translated instruction streams) and
are intentionally not recorded here.
