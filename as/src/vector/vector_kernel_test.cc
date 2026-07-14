/*
 * vector_kernel_test.cc
 *
 * EC528: kernel table selection and scalar-row tests.
 */
#include <gtest/gtest.h>

#include <cstdint>

#include "vector/sptag_distance.h"
#include "vector/vector_distance.h"
#include "vector/vector_kernel.h"

namespace {

// An ISA that can never be registered in this build: x86 rows are
// arch-excluded on aarch64 and NEON is arch-excluded on x86.
#if defined(__aarch64__) || defined(_M_ARM64)
constexpr as_vector_isa kForeignIsa = AS_VECTOR_ISA_AVX2;
constexpr const char* kForeignMode = "avx2";
#else
constexpr as_vector_isa kForeignIsa = AS_VECTOR_ISA_NEON;
constexpr const char* kForeignMode = "neon";
#endif

TEST(VectorKernelSelect, ExplicitScalar)
{
	as_vector_isa isa = AS_VECTOR_ISA_COUNT;
	ASSERT_EQ(0, as_vector_kernel_choose("scalar", &isa));
	EXPECT_EQ(AS_VECTOR_ISA_SCALAR, isa);
}

TEST(VectorKernelSelect, AutoPicksAvailableAndMatchesDefault)
{
	as_vector_isa auto_isa = AS_VECTOR_ISA_COUNT;
	ASSERT_EQ(0, as_vector_kernel_choose("auto", &auto_isa));
	EXPECT_TRUE(as_vector_kernel_available(auto_isa));

	as_vector_isa null_isa = AS_VECTOR_ISA_COUNT;
	ASSERT_EQ(0, as_vector_kernel_choose(nullptr, &null_isa));
	EXPECT_EQ(auto_isa, null_isa);

	as_vector_isa empty_isa = AS_VECTOR_ISA_COUNT;
	ASSERT_EQ(0, as_vector_kernel_choose("", &empty_isa));
	EXPECT_EQ(auto_isa, empty_isa);
}

TEST(VectorKernelSelect, UnknownModeIsError)
{
	as_vector_isa isa = AS_VECTOR_ISA_COUNT;
	EXPECT_EQ(-1, as_vector_kernel_choose("bogus", &isa));
	// Exact lowercase only - no case-insensitive fallback.
	EXPECT_EQ(-1, as_vector_kernel_choose("SCALAR", &isa));
	EXPECT_EQ(-1, as_vector_kernel_choose(" scalar", &isa));
	// Error paths must not write the out param.
	EXPECT_EQ(AS_VECTOR_ISA_COUNT, isa);
}

TEST(VectorKernelSelect, ForeignIsaUnavailableNoFallback)
{
	EXPECT_FALSE(as_vector_kernel_available(kForeignIsa));

	as_vector_isa isa = AS_VECTOR_ISA_COUNT;
	EXPECT_EQ(-2, as_vector_kernel_choose(kForeignMode, &isa));
	EXPECT_EQ(AS_VECTOR_ISA_COUNT, isa);
}

TEST(VectorKernelTable, ScalarRowMatchesReference)
{
	const float fx[7] = { 1.5f, -2.0f, 0.0f, 3.25f, -0.5f, 8.0f, 2.0f };
	const float fy[7] = { 0.5f, 2.0f, -1.0f, 3.0f, 0.5f, -8.0f, 2.5f };

	as_vector_kernel_fn l2f = as_vector_kernel_get(AS_VECTOR_ISA_SCALAR,
			AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_L2);
	ASSERT_NE(nullptr, l2f);
	EXPECT_EQ(sptag::DistanceUtils::ComputeL2Distance(fx, fy, 7),
			l2f(fx, fy, 7));

	as_vector_kernel_fn cosf = as_vector_kernel_get(AS_VECTOR_ISA_SCALAR,
			AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_COSINE);
	ASSERT_NE(nullptr, cosf);
	EXPECT_EQ(sptag::DistanceUtils::ComputeCosineDistance(fx, fy, 7),
			cosf(fx, fy, 7));

	const int8_t ix[5] = { 1, -2, 3, -4, 5 };
	const int8_t iy[5] = { -1, 2, -3, 4, -5 };

	as_vector_kernel_fn l2i = as_vector_kernel_get(AS_VECTOR_ISA_SCALAR,
			AS_VECTOR_VALUE_TYPE_INT8, AS_VECTOR_METRIC_L2);
	ASSERT_NE(nullptr, l2i);
	EXPECT_EQ(sptag::DistanceUtils::ComputeL2Distance(ix, iy, 5),
			l2i(ix, iy, 5));

	// Inner-product shares the cosine-family kernel.
	EXPECT_EQ(as_vector_kernel_get(AS_VECTOR_ISA_SCALAR,
					AS_VECTOR_VALUE_TYPE_INT8, AS_VECTOR_METRIC_COSINE),
			as_vector_kernel_get(AS_VECTOR_ISA_SCALAR,
					AS_VECTOR_VALUE_TYPE_INT8,
					AS_VECTOR_METRIC_INNER_PRODUCT));
}

TEST(VectorKernelTable, RejectsBadEnums)
{
	EXPECT_EQ(nullptr, as_vector_kernel_get(AS_VECTOR_ISA_SCALAR,
			AS_VECTOR_VALUE_TYPE_BAD, AS_VECTOR_METRIC_L2));
	EXPECT_EQ(nullptr, as_vector_kernel_get(AS_VECTOR_ISA_SCALAR,
			AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_BAD));
	EXPECT_EQ(nullptr, as_vector_kernel_get(AS_VECTOR_ISA_COUNT,
			AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_L2));
	EXPECT_EQ(nullptr, as_vector_kernel_get(kForeignIsa,
			AS_VECTOR_VALUE_TYPE_FLOAT, AS_VECTOR_METRIC_L2));
}

TEST(VectorKernelTable, PublicComputeStaysPinnedToScalar)
{
	const float fx[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
	const float fy[4] = { 4.0f, 3.0f, 2.0f, 1.0f };
	float out = 0.0f;

	ASSERT_EQ(0, as_vector_distance_compute(AS_VECTOR_VALUE_TYPE_FLOAT,
			AS_VECTOR_METRIC_L2, 4, fx, fy, &out));
	EXPECT_EQ(sptag::DistanceUtils::ComputeL2Distance(fx, fy, 4), out);

	// Old failure contract preserved.
	EXPECT_EQ(-1, as_vector_distance_compute(AS_VECTOR_VALUE_TYPE_BAD,
			AS_VECTOR_METRIC_L2, 4, fx, fy, &out));
	EXPECT_EQ(-1, as_vector_distance_compute(AS_VECTOR_VALUE_TYPE_FLOAT,
			AS_VECTOR_METRIC_BAD, 4, fx, fy, &out));
	EXPECT_EQ(-1, as_vector_distance_compute(AS_VECTOR_VALUE_TYPE_FLOAT,
			AS_VECTOR_METRIC_L2, 0, fx, fy, &out));
	EXPECT_EQ(-1, as_vector_distance_compute(AS_VECTOR_VALUE_TYPE_FLOAT,
			AS_VECTOR_METRIC_L2, 4, nullptr, fy, &out));
}

} // namespace
