/*
 * vector_topk_test.cc
 *
 * SPEC-3-TOPK-001: Owner-local top K ordering and dedupe.
 */
#include <gtest/gtest.h>

#include <vector>

#include "vector/vector_topk.h"

TEST(VectorTopK, DedupeByVidKeepsBest) // SPEC-3-TOPK-001
{
	as_vector_scored_tail storage[4];
	as_vector_topk acc;

	as_vector_topk_init(&acc, storage, 4, 2);

	as_vector_scored_tail a = { 1, 10, 0, 0.5f };
	as_vector_scored_tail b = { 2, 10, 0, 0.2f };
	as_vector_scored_tail c = { 3, 20, 0, 0.4f };

	as_vector_topk_add(&acc, &a);
	as_vector_topk_add(&acc, &b);
	as_vector_topk_add(&acc, &c);

	as_vector_scored_tail out[2];
	uint32_t n = as_vector_topk_fill(&acc, out, 2);

	ASSERT_EQ(2u, n);
	EXPECT_EQ(10, out[0].vid);
	EXPECT_FLOAT_EQ(0.2f, out[0].distance);
	EXPECT_EQ(20, out[1].vid);
}

TEST(VectorTopK, TieBreakVidThenHead) // SPEC-3-TOPK-001
{
	as_vector_scored_tail storage[3];
	as_vector_topk acc;

	as_vector_topk_init(&acc, storage, 3, 2);

	as_vector_scored_tail a = { 2, 20, 0, 0.1f };
	as_vector_scored_tail b = { 1, 10, 0, 0.1f };

	as_vector_topk_add(&acc, &a);
	as_vector_topk_add(&acc, &b);

	as_vector_scored_tail out[2];
	uint32_t n = as_vector_topk_fill(&acc, out, 2);

	ASSERT_EQ(2u, n);
	EXPECT_EQ(10, out[0].vid);
	EXPECT_EQ(20, out[1].vid);
}
