/*
 * vector_kernel.h
 *
 * EC528: runtime kernel dispatch table for VECTOR_DISTANCE tail scoring.
 *
 * The table maps (ISA, value type, metric family) to a distance kernel.
 * SCALAR is always registered; SIMD translation units register their rows
 * from constructors. Selection is driven by the AEROSPIKE_VECTOR_SIMD
 * environment value ("auto"|"scalar"|"sse"|"avx2"|"avx512"|"neon", exact
 * lowercase). There is no silent fallback: an unknown mode or an explicitly
 * requested ISA that is not available in this build/CPU is a selection
 * error the caller must treat as fatal.
 *
 * This file must stay free of server dependencies (no cf_ or base code):
 * it is compiled into the standalone vector unit-test binary.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "vector/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef float (*as_vector_kernel_fn)(const void* x, const void* y,
		uint32_t dim);

typedef enum {
	AS_VECTOR_ISA_SCALAR = 0,
	AS_VECTOR_ISA_SSE = 1,
	AS_VECTOR_ISA_AVX2 = 2,
	AS_VECTOR_ISA_AVX512 = 3,
	AS_VECTOR_ISA_NEON = 4,

	AS_VECTOR_ISA_COUNT = 5
} as_vector_isa;

typedef enum {
	AS_VECTOR_KERNEL_FAMILY_L2 = 0,
	AS_VECTOR_KERNEL_FAMILY_COSINE = 1, // cosine and inner-product share it

	AS_VECTOR_KERNEL_FAMILY_COUNT = 2
} as_vector_kernel_family;

#define AS_VECTOR_KERNEL_TYPE_COUNT 4 // float, uint8, int8, int16

// Returns true when the ISA row can execute on this process: registered in
// this build and its runtime-support predicate (if any) passes.
bool
as_vector_kernel_available(as_vector_isa isa);

// Selection. mode NULL/""/"auto" picks the best available ISA (highest
// enum value). An explicit mode names one ISA which must be available.
// Returns 0 on success, -1 for an unknown mode string, -2 for a known mode
// whose ISA is not available. No fallback on error; *out is untouched.
int
as_vector_kernel_choose(const char* mode, as_vector_isa* out);

// Table lookup. Returns NULL for an unregistered ISA row or invalid
// type/metric. Not gated on runtime support - callers select via
// as_vector_kernel_choose() first.
as_vector_kernel_fn
as_vector_kernel_get(as_vector_isa isa, as_vector_value_type value_type,
		as_vector_metric metric);

typedef bool (*as_vector_isa_supported_fn)(void);

// Registration hook for SIMD translation units (constructor time, before
// main; not thread-safe afterwards by design). fns is indexed
// [value type][metric family]; supported NULL means always supported.
void
as_vector_kernel_register(as_vector_isa isa,
		as_vector_isa_supported_fn supported,
		const as_vector_kernel_fn
				fns[AS_VECTOR_KERNEL_TYPE_COUNT][AS_VECTOR_KERNEL_FAMILY_COUNT]);

const char*
as_vector_isa_name(as_vector_isa isa);

#ifdef __cplusplus
}
#endif
