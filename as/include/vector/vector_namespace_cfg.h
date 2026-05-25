/*
 * vector_namespace_cfg.h
 *
 * EC528: minimal namespace vector config surface for unit tests.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "vector/vector_types.h"

typedef struct as_vector_namespace_cfg_s {
	uint32_t vector_dimension;
	uint8_t vector_value_type;
	uint8_t vector_metric;
	uint32_t vector_max_query_bytes;
} as_vector_namespace_cfg;

#ifdef __cplusplus
extern "C" {
#endif

bool
as_vector_namespace_cfg_valid(const as_vector_namespace_cfg* ns);

#ifdef __cplusplus
}
#endif
