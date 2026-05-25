/*
 * vector_topk.c
 *
 * EC528: Owner-local top-K accumulator.
 */
#include "vector/vector_topk.h"

#include <string.h>

static int
compare_scored(const as_vector_scored_tail* a, const as_vector_scored_tail* b)
{
	if (a->distance < b->distance) {
		return -1;
	}
	if (a->distance > b->distance) {
		return 1;
	}
	if (a->vid < b->vid) {
		return -1;
	}
	if (a->vid > b->vid) {
		return 1;
	}
	if (a->head_id_key < b->head_id_key) {
		return -1;
	}
	if (a->head_id_key > b->head_id_key) {
		return 1;
	}
	if (a->version < b->version) {
		return -1;
	}
	if (a->version > b->version) {
		return 1;
	}
	return 0;
}

static int
find_vid(const as_vector_topk* topk, int32_t vid)
{
	for (uint32_t i = 0; i < topk->count; i++) {
		if (topk->items[i].vid == vid) {
			return (int)i;
		}
	}
	return -1;
}

void
as_vector_topk_init(as_vector_topk* topk, as_vector_scored_tail* storage,
		uint32_t capacity, uint32_t topk_limit)
{
	topk->items = storage;
	topk->capacity = capacity;
	topk->count = 0;
	topk->topk = topk_limit;
}

void
as_vector_topk_add(as_vector_topk* topk, const as_vector_scored_tail* item)
{
	int ix = find_vid(topk, item->vid);

	if (ix >= 0) {
		if (compare_scored(&topk->items[ix], item) <= 0) {
			return;
		}
		topk->items[ix] = *item;
	}
	else if (topk->count < topk->capacity) {
		topk->items[topk->count++] = *item;
	}
	else {
		// Replace worst if better.
		uint32_t worst = 0;

		for (uint32_t i = 1; i < topk->count; i++) {
			if (compare_scored(&topk->items[i], &topk->items[worst]) > 0) {
				worst = i;
			}
		}

		if (compare_scored(item, &topk->items[worst]) >= 0) {
			return;
		}

		topk->items[worst] = *item;
	}

	// If over topk, drop worst.
	while (topk->count > topk->topk) {
		uint32_t worst = 0;

		for (uint32_t i = 1; i < topk->count; i++) {
			if (compare_scored(&topk->items[i], &topk->items[worst]) > 0) {
				worst = i;
			}
		}
		topk->items[worst] = topk->items[topk->count - 1];
		topk->count--;
	}
}

static void
sort_items(as_vector_scored_tail* items, uint32_t count)
{
	for (uint32_t i = 0; i + 1 < count; i++) {
		for (uint32_t j = i + 1; j < count; j++) {
			if (compare_scored(&items[j], &items[i]) < 0) {
				as_vector_scored_tail tmp = items[i];
				items[i] = items[j];
				items[j] = tmp;
			}
		}
	}
}

uint32_t
as_vector_topk_fill(const as_vector_topk* topk, as_vector_scored_tail* out,
		uint32_t out_cap)
{
	uint32_t n = topk->count < out_cap ? topk->count : out_cap;

	memcpy(out, topk->items, n * sizeof(as_vector_scored_tail));
	sort_items(out, n);
	return n;
}
