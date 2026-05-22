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

template<typename T>
static inline bool
compute_typed(as_vector_metric metric, const T* x, const T* y, uint32_t dim,
		float* out)
{
	switch (metric) {
	case AS_VECTOR_METRIC_L2:
		*out = sptag::DistanceUtils::ComputeL2Distance(x, y, dim);
		return true;
	case AS_VECTOR_METRIC_COSINE:
	case AS_VECTOR_METRIC_INNER_PRODUCT:
		// EC528: SPTAG cosine returns base*base - dot, which is genuinely
		// negative when vectors are strongly aligned. Smaller-is-better;
		// negative values are valid distances, not failures.
		*out = sptag::DistanceUtils::ComputeCosineDistance(x, y, dim);
		return true;
	default:
		return false;
	}
}

extern "C" int
as_vector_distance_compute(as_vector_value_type value_type,
		as_vector_metric metric, uint32_t dimension, const void* query,
		const void* tail, float* distance_out)
{
	if (query == NULL || tail == NULL || distance_out == NULL || dimension == 0) {
		return -1;
	}

	bool ok = false;

	switch (value_type) {
	case AS_VECTOR_VALUE_TYPE_FLOAT:
		ok = compute_typed<float>(metric, (const float*)query,
				(const float*)tail, dimension, distance_out);
		break;
	case AS_VECTOR_VALUE_TYPE_UINT8:
		ok = compute_typed<uint8_t>(metric, (const uint8_t*)query,
				(const uint8_t*)tail, dimension, distance_out);
		break;
	case AS_VECTOR_VALUE_TYPE_INT8:
		ok = compute_typed<int8_t>(metric, (const int8_t*)query,
				(const int8_t*)tail, dimension, distance_out);
		break;
	case AS_VECTOR_VALUE_TYPE_INT16:
		ok = compute_typed<int16_t>(metric, (const int16_t*)query,
				(const int16_t*)tail, dimension, distance_out);
		break;
	default:
		return -1;
	}

	return ok ? 0 : -1;
}
