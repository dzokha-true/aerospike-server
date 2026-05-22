/*
 * vector_math_test.cc
 *
 * SPEC-3-VDIST-001: SPTAG ComputeDistance parity.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "vector/sptag_distance.h"
#include "vector/vector_distance.h"

namespace {

template<typename T>
float
ref_l2(const T* x, const T* y, uint32_t dim)
{
	return sptag::DistanceUtils::ComputeL2Distance(x, y, dim);
}

template<typename T>
float
ref_cosine(const T* x, const T* y, uint32_t dim)
{
	return sptag::DistanceUtils::ComputeCosineDistance(x, y, dim);
}

template<typename T>
void
expect_metric(as_vector_value_type vt, as_vector_metric metric, const T* q,
		const T* tail, uint32_t dim, float expected)
{
	float got = -1.0f;

	ASSERT_EQ(0, as_vector_distance_compute(vt, metric, dim, q, tail, &got));
	EXPECT_FLOAT_EQ(expected, got);
}

} // namespace

TEST(VectorMath, FloatL2) // SPEC-3-VDIST-001
{
	const float q[] = { 0.0f, 0.0f };
	const float t[] = { 3.0f, 4.0f };
	const float expected = ref_l2(q, t, 2);

	expect_metric(AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_L2, q, t, 2,
			expected);
}

TEST(VectorMath, FloatCosineAndInnerProduct) // SPEC-3-VDIST-001
{
	const float q[] = { 1.0f, 0.0f };
	const float t[] = { 0.0f, 1.0f };
	const float expected = ref_cosine(q, t, 2);

	expect_metric(AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_COSINE, q, t, 2,
			expected);
	expect_metric(AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_INNER_PRODUCT, q,
			t, 2, expected);
}

TEST(VectorMath, UInt8L2) // SPEC-3-VDIST-001
{
	const uint8_t q[] = { 0, 0 };
	const uint8_t t[] = { 3, 4 };
	const float expected = ref_l2(q, t, 2);

	expect_metric(AS_VECTOR_VALUE_TYPE_UINT8, AS_VECTOR_METRIC_L2, q, t, 2,
			expected);
}

TEST(VectorMath, Int16Cosine) // SPEC-3-VDIST-001
{
	const int16_t q[] = { 100, 0 };
	const int16_t t[] = { 0, 100 };
	const float expected = ref_cosine(q, t, 2);

	expect_metric(AS_VECTOR_VALUE_TYPE_INT16, AS_VECTOR_METRIC_COSINE, q, t, 2,
			expected);
}

TEST(VectorMath, NegativeCosineIsValid) // SPEC-3-VDIST-001
{
	// Strongly-aligned non-unit vectors -> dot > 1 -> SPTAG cosine returns
	// a negative score, which means very similar (smaller-is-better). The
	// wrapper must not treat that as a compute failure.
	const float q[] = { 5.0f, 5.0f };
	const float t[] = { 5.0f, 5.0f };
	float got = 0.0f;

	ASSERT_EQ(0, as_vector_distance_compute(AS_VECTOR_VALUE_TYPE_FLOAT,
			AS_VECTOR_METRIC_COSINE, 2, q, t, &got));
	EXPECT_LT(got, 0.0f);
	EXPECT_FLOAT_EQ(1.0f - 50.0f, got);
}

TEST(VectorMath, BadDimension) // SPEC-3-VDIST-001
{
	const float q[] = { 1.0f };
	float d = 0.0f;

	EXPECT_NE(0, as_vector_distance_compute(AS_VECTOR_VALUE_TYPE_FLOAT,
			AS_VECTOR_METRIC_L2, 0, q, q, &d));
}
