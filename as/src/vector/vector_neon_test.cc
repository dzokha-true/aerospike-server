/*
 * vector_neon_test.cc
 *
 * EC528: NEON kernel parity vs the scalar oracle.
 *
 * Tolerance policy: NEON accumulates in float lanes and reduces once, so
 * float addition order differs from the sequential scalar loop. All types
 * and metrics are therefore compared with relative tolerance 1e-5 (absolute
 * floor 1e-5 near zero); integer inputs go through the same float
 * accumulator on both sides, so they get the same bound rather than exact
 * equality.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "vector/vector_distance.h"
#include "vector/vector_kernel.h"

#if !defined(__aarch64__) && !defined(_M_ARM64)

TEST(VectorNeon, SkippedOnNonArm64)
{
	GTEST_SKIP() << "NEON kernels are aarch64-only";
}

#else

namespace {

constexpr uint32_t kDims[] = { 1, 3, 4, 7, 8, 15, 16, 31, 32, 33, 64, 100,
		128, 768 };
constexpr uint32_t kSeed = 528528;


// Accumulation-order bound: both the scalar oracle and the SIMD kernels sum
// per-element float terms, but in different orders. Under cancellation
// (signed dot products) the order-dependent error is bounded by
// n * max_term * epsilon; max_term is 4*base^2 (covers L2 diffs and dots).
inline float
accum_tol(as_vector_value_type vt, uint32_t dim)
{
	float b = vt == AS_VECTOR_VALUE_TYPE_FLOAT ? 1.0f :
			vt == AS_VECTOR_VALUE_TYPE_UINT8 ? 255.0f :
			vt == AS_VECTOR_VALUE_TYPE_INT8 ? 127.0f : 32767.0f;
	return 4.0f * b * b * (float)dim * 1.1920929e-07f;
}

void
expect_close(float expected, float actual, const char* what,
		as_vector_value_type vt, uint32_t dim)
{
	float tol = 1e-5f * std::max({ 1.0f, std::abs(expected),
			std::abs(actual) }) + accum_tol(vt, dim);
	EXPECT_NEAR(expected, actual, tol)
			<< what << " dim=" << dim << " seed=" << kSeed;
}

template<typename T>
std::vector<T>
random_vec(std::mt19937& rng, uint32_t dim)
{
	std::vector<T> v(dim);

	if (std::is_same<T, float>::value) {
		std::uniform_real_distribution<float> d(-1.0f, 1.0f);
		for (auto& e : v) {
			e = (T)d(rng);
		}
	}
	else {
		std::uniform_int_distribution<int32_t> d(
				(int32_t)std::numeric_limits<T>::min(),
				(int32_t)std::numeric_limits<T>::max());
		for (auto& e : v) {
			e = (T)d(rng);
		}
	}

	return v;
}

template<typename T>
void
parity_case(as_vector_value_type vt, const char* label)
{
	std::mt19937 rng(kSeed);

	for (uint32_t dim : kDims) {
		auto x = random_vec<T>(rng, dim);
		auto y = random_vec<T>(rng, dim);

		for (as_vector_metric metric : { AS_VECTOR_METRIC_L2,
				AS_VECTOR_METRIC_COSINE, AS_VECTOR_METRIC_INNER_PRODUCT }) {
			as_vector_kernel_fn neon = as_vector_kernel_get(
					AS_VECTOR_ISA_NEON, vt, metric);
			ASSERT_NE(nullptr, neon) << label;

			float expected = 0.0f;
			ASSERT_EQ(0, as_vector_distance_compute(vt, metric, dim,
					x.data(), y.data(), &expected));

			expect_close(expected, neon(x.data(), y.data(), dim), label,
					vt, dim);
		}
	}
}

TEST(VectorNeon, RegisteredAndAutoSelected)
{
	EXPECT_TRUE(as_vector_kernel_available(AS_VECTOR_ISA_NEON));

	as_vector_isa isa = AS_VECTOR_ISA_COUNT;
	ASSERT_EQ(0, as_vector_kernel_choose("auto", &isa));
	EXPECT_EQ(AS_VECTOR_ISA_NEON, isa) << "auto must pick NEON on aarch64";

	ASSERT_EQ(0, as_vector_kernel_choose("neon", &isa));
	EXPECT_EQ(AS_VECTOR_ISA_NEON, isa);

	// Explicit scalar still forces scalar - no override.
	ASSERT_EQ(0, as_vector_kernel_choose("scalar", &isa));
	EXPECT_EQ(AS_VECTOR_ISA_SCALAR, isa);
}

TEST(VectorNeon, ParityFloat)
{
	parity_case<float>(AS_VECTOR_VALUE_TYPE_FLOAT, "float");
}

TEST(VectorNeon, ParityUint8)
{
	parity_case<uint8_t>(AS_VECTOR_VALUE_TYPE_UINT8, "uint8");
}

TEST(VectorNeon, ParityInt8)
{
	parity_case<int8_t>(AS_VECTOR_VALUE_TYPE_INT8, "int8");
}

TEST(VectorNeon, ParityInt16)
{
	parity_case<int16_t>(AS_VECTOR_VALUE_TYPE_INT16, "int16");
}

} // namespace

#endif // __aarch64__
