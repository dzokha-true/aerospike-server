/*
 * vector_topk.h
 *
 * EC528: Owner-local top-K accumulator.
 */
#pragma once

#include <stdint.h>

#include "vector/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct as_vector_scored_tail_s {
	int64_t head_id_key;
	int32_t vid;
	uint8_t version;
	float distance;
} as_vector_scored_tail;

typedef struct as_vector_topk_s {
	as_vector_scored_tail* items;
	uint32_t capacity;
	uint32_t count;
	uint32_t topk;
} as_vector_topk;

void
as_vector_topk_init(as_vector_topk* topk, as_vector_scored_tail* storage,
		uint32_t capacity, uint32_t topk_limit);

void
as_vector_topk_add(as_vector_topk* topk, const as_vector_scored_tail* item);

uint32_t
as_vector_topk_fill(const as_vector_topk* topk, as_vector_scored_tail* out,
		uint32_t out_cap);

#ifdef __cplusplus
}
#endif
