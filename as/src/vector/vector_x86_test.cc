/*
 * vector_x86_test.cc
 *
 * EC528: x86 SIMD kernel parity vs the scalar oracle. Each ISA suite is
 * runtime-gated: unsupported ISAs GTEST_SKIP (expected for AVX-512 under
 * Rosetta emulation). Tolerance policy matches the NEON tests: relative
 * 1e-5 with a 1.0 absolute floor, because lane-parallel float accumulation
 * reorders additions relative to the sequential scalar loop.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <vector>

#include "vector/vector_cpu.h"
#include "vector/vector_distance.h"
#include "vector/vector_kernel.h"

#if !defined(__x86_64__) && !defined(_M_X64)

TEST(VectorX86, SkippedOnNonX86)
{
	GTEST_SKIP() << "x86 SIMD kernels are x86-64-only";
}

#else

namespace {

constexpr uint32_t kDims[] = { 1, 3, 4, 7, 8, 15, 16, 31, 32, 33, 64, 100,
		128, 768 };
constexpr uint32_t kSeed = 528529;

void
expect_close(float expected, float actual, const char* what, uint32_t dim)
{
	float tol = 1e-5f * std::max({ 1.0f, std::abs(expected),
			std::abs(actual) });
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
parity_type(as_vector_isa isa, as_vector_value_type vt, const char* label)
{
	std::mt19937 rng(kSeed);

	for (uint32_t dim : kDims) {
		auto x = random_vec<T>(rng, dim);
		auto y = random_vec<T>(rng, dim);

		for (as_vector_metric metric : { AS_VECTOR_METRIC_L2,
				AS_VECTOR_METRIC_COSINE, AS_VECTOR_METRIC_INNER_PRODUCT }) {
			as_vector_kernel_fn fn = as_vector_kernel_get(isa, vt, metric);
			ASSERT_NE(nullptr, fn) << label;

			float expected = 0.0f;
			ASSERT_EQ(0, as_vector_distance_compute(vt, metric, dim,
					x.data(), y.data(), &expected));

			expect_close(expected, fn(x.data(), y.data(), dim), label, dim);
		}
	}
}

void
parity_all_types(as_vector_isa isa)
{
	parity_type<float>(isa, AS_VECTOR_VALUE_TYPE_FLOAT, "float");
	parity_type<uint8_t>(isa, AS_VECTOR_VALUE_TYPE_UINT8, "uint8");
	parity_type<int8_t>(isa, AS_VECTOR_VALUE_TYPE_INT8, "int8");
	parity_type<int16_t>(isa, AS_VECTOR_VALUE_TYPE_INT16, "int16");
}

TEST(VectorX86, DetectionReport)
{
	// Always runs on x86: records what the host/emulator exposes so the
	// check log shows which parity suites were live.
	printf("cpu: sse41=%d avx2=%d avx512bw=%d\n",
			(int)as_vector_cpu_has_sse41(), (int)as_vector_cpu_has_avx2(),
			(int)as_vector_cpu_has_avx512bw());

	// Registration is unconditional on x86 builds; availability is
	// runtime-gated.
	as_vector_isa isa = AS_VECTOR_ISA_COUNT;
	ASSERT_EQ(0, as_vector_kernel_choose("auto", &isa));
	EXPECT_TRUE(as_vector_kernel_available(isa));
	SUCCEED();
}

TEST(VectorX86, ParitySse)
{
	if (! as_vector_kernel_available(AS_VECTOR_ISA_SSE)) {
		GTEST_SKIP() << "SSE4.1 not available";
	}
	parity_all_types(AS_VECTOR_ISA_SSE);
}

TEST(VectorX86, ParityAvx2)
{
	if (! as_vector_kernel_available(AS_VECTOR_ISA_AVX2)) {
		GTEST_SKIP() << "AVX2 not available";
	}
	parity_all_types(AS_VECTOR_ISA_AVX2);
}

TEST(VectorX86, ParityAvx512)
{
	if (! as_vector_kernel_available(AS_VECTOR_ISA_AVX512)) {
		GTEST_SKIP() << "AVX-512 not available (expected under Rosetta)";
	}
	parity_all_types(AS_VECTOR_ISA_AVX512);
}

} // namespace

#endif // x86-64
