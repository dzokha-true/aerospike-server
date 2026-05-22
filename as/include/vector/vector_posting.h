/*
 * vector_posting.h
 *
 * EC528: SPTAG posting blob parser.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "vector/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct as_vector_posting_element_s {
	int32_t vid;
	uint8_t version;
	const uint8_t* payload;
} as_vector_posting_element;

typedef struct as_vector_posting_iter_s {
	const uint8_t* blob;
	uint32_t blob_size;
	uint32_t stride;
	uint32_t offset;
} as_vector_posting_iter;

bool
as_vector_posting_stride(uint32_t dimension, as_vector_value_type value_type,
		uint32_t* stride_out);

bool
as_vector_posting_iter_init(as_vector_posting_iter* it, const uint8_t* blob,
		uint32_t blob_size, uint32_t dimension,
		as_vector_value_type value_type);

bool
as_vector_posting_iter_next(as_vector_posting_iter* it,
		as_vector_posting_element* elem_out);

#ifdef __cplusplus
}
#endif
