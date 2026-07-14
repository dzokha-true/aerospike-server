/*
 * vector_distance.cc
 *
 * EC528: SPTAG-compatible distance (scalar + optional vendored SIMD).
 *
 * Portions adapted from Microsoft SPTAG DistanceUtils (MIT License).
 */
#include "vector/vector_distance.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "vector/sptag_distance.h" // EC528: MIT-adapted SPTAG scalar distance
#include "vector/vector_kernel.h"

// EC528: C-linkage scalar kernels - the SCALAR row of the kernel table and
// the canonical reference implementation SIMD rows are validated against.

#define SCALAR_KERNEL(name, type, method) \
	extern "C" float \
	name(const void* x, const void* y, uint32_t dim) \
	{ \
		return sptag::DistanceUtils::method((const type*)x, (const type*)y, \
				dim); \
	}

SCALAR_KERNEL(as_vector_scalar_l2_float, float, ComputeL2Distance)
SCALAR_KERNEL(as_vector_scalar_cos_float, float, ComputeCosineDistance)
SCALAR_KERNEL(as_vector_scalar_l2_uint8, uint8_t, ComputeL2Distance)
SCALAR_KERNEL(as_vector_scalar_cos_uint8, uint8_t, ComputeCosineDistance)
SCALAR_KERNEL(as_vector_scalar_l2_int8, int8_t, ComputeL2Distance)
SCALAR_KERNEL(as_vector_scalar_cos_int8, int8_t, ComputeCosineDistance)
SCALAR_KERNEL(as_vector_scalar_l2_int16, int16_t, ComputeL2Distance)
SCALAR_KERNEL(as_vector_scalar_cos_int16, int16_t, ComputeCosineDistance)

// EC528: SPTAG cosine returns base*base - dot, which is genuinely negative
// when vectors are strongly aligned. Smaller-is-better; negative values are
// valid distances, not failures.
//
// This entry point is pinned to the SCALAR row on purpose: it is the
// reference oracle for SIMD parity tests and keeps bit-stable results
// regardless of AEROSPIKE_VECTOR_SIMD. The production hot path resolves its
// kernel through as_vector_kernel_choose()/as_vector_kernel_get().
extern "C" int
as_vector_distance_compute(as_vector_value_type value_type,
		as_vector_metric metric, uint32_t dimension, const void* query,
		const void* tail, float* distance_out)
{
	if (query == NULL || tail == NULL || distance_out == NULL || dimension == 0) {
		return -1;
	}

	as_vector_kernel_fn fn = as_vector_kernel_get(AS_VECTOR_ISA_SCALAR,
			value_type, metric);

	if (fn == NULL) {
		return -1;
	}

	*distance_out = fn(query, tail, dimension);

	return 0;
}
