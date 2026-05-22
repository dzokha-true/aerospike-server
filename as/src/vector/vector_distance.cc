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
static inline int
sptag_base()
{
	if (sizeof(T) == sizeof(float)) {
		return 1;
	}
	return (int)std::numeric_limits<T>::max();
}

template<typename T>
static float
l2_scalar(const T* x, const T* y, uint32_t dim)
{
	float diff = 0.0f;

	for (uint32_t i = 0; i < dim; i++) {
		float d = (float)x[i] - (float)y[i];
		diff += d * d;
	}
	return diff;
}

template<typename T>
static float
cosine_family_scalar(const T* x, const T* y, uint32_t dim)
{
	const int base = sptag_base<T>();
	float dot = 0.0f;

	for (uint32_t i = 0; i < dim; i++) {
		dot += (float)x[i] * (float)y[i];
	}
	return (float)(base * base) - dot;
}

template<typename T>
static float
compute_typed(as_vector_metric metric, const T* x, const T* y, uint32_t dim)
{
	switch (metric) {
	case AS_VECTOR_METRIC_L2:
		return sptag::DistanceUtils::ComputeL2Distance(x, y, dim);
	case AS_VECTOR_METRIC_COSINE:
	case AS_VECTOR_METRIC_INNER_PRODUCT:
		return sptag::DistanceUtils::ComputeCosineDistance(x, y, dim);
	default:
		return -1.0f;
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

	float d = -1.0f;

	switch (value_type) {
	case AS_VECTOR_VALUE_TYPE_FLOAT:
		d = compute_typed<float>(metric, (const float*)query, (const float*)tail,
				dimension);
		break;
	case AS_VECTOR_VALUE_TYPE_UINT8:
		d = compute_typed<uint8_t>(metric, (const uint8_t*)query,
				(const uint8_t*)tail, dimension);
		break;
	case AS_VECTOR_VALUE_TYPE_INT8:
		d = compute_typed<int8_t>(metric, (const int8_t*)query, (const int8_t*)tail,
				dimension);
		break;
	case AS_VECTOR_VALUE_TYPE_INT16:
		d = compute_typed<int16_t>(metric, (const int16_t*)query,
				(const int16_t*)tail, dimension);
		break;
	default:
		return -1;
	}

	if (d < 0.0f) {
		return -1;
	}

	*distance_out = d;
	return 0;
}
