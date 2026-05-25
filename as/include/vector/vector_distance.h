/*
 * vector_distance.h
 *
 * EC528: SPTAG-compatible distance functions.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "vector/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int
as_vector_distance_compute(as_vector_value_type value_type,
		as_vector_metric metric, uint32_t dimension, const void* query,
		const void* tail, float* distance_out);

#ifdef __cplusplus
}
#endif
