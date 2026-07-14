/*
 * vector_cpu.h
 *
 * EC528: x86 CPU feature detection for SIMD kernel selection. On non-x86
 * builds every probe is constant false. Kept free of server dependencies -
 * compiled into the standalone vector unit-test binary.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// SSE row baseline: SSE4.1 (pmovzx/pmovsx widening in the 8-bit kernels).
bool
as_vector_cpu_has_sse41(void);

// AVX2 row: OSXSAVE + XCR0 xmm/ymm enabled + AVX + AVX2.
bool
as_vector_cpu_has_avx2(void);

// AVX-512 row: AVX2 prerequisites + XCR0 opmask/zmm enabled + AVX512F +
// AVX512BW (the integer kernels need BW).
bool
as_vector_cpu_has_avx512bw(void);

#ifdef __cplusplus
}
#endif
