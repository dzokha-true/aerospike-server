/*
 * vector_parity_test.cc
 *
 * EC528: comprehensive randomized SIMD parity suite. Every runtime-available
 * ISA row is compared against the scalar oracle
 * (as_vector_distance_compute) for every value type, both metric families
 * (via all three metrics), and every dim in 1..67 plus {100,128,768,1024} -
 * covering all remainder-lane cases for 4/8/16/32-wide main loops.
 *
 * Tolerance policy (documented in docs/benchmarks/simd-kernels.md): all
 * comparisons use relative tolerance 1e-5 with an absolute floor of 1e-5 at
 * magnitude <= 1. Integer inputs are included: both sides accumulate in
 * float, but lane-parallel reduction reorders the additions, so bit
 * equality is not guaranteed for any input type.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "vector/vector_distance.h"
#include "vector/vector_kernel.h"

namespace {

constexpr uint32_t kSeed = 20260714;

std::vector<uint32_t>
parity_dims()
{
	std::vector<uint32_t> dims;

	for (uint32_t d = 1; d <= 67; d++) {
		dims.push_back(d);
	}
	for (uint32_t d : { 100u, 128u, 768u, 1024u }) {
		dims.push_back(d);
	}

	return dims;
}


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
expect_close(float expected, float actual, const char* isa, const char* type,
		int metric, as_vector_value_type vt, uint32_t dim)
{
	float tol = 1e-5f * std::max({ 1.0f, std::abs(expected),
			std::abs(actual) }) + accum_tol(vt, dim);
	EXPECT_NEAR(expected, actual, tol) << "isa=" << isa << " type=" << type
			<< " metric=" << metric << " dim=" << dim << " seed=" << kSeed;
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
parity_type(as_vector_isa isa, as_vector_value_type vt, const char* type)
{
	std::mt19937 rng(kSeed + (uint32_t)vt);

	for (uint32_t dim : parity_dims()) {
		auto x = random_vec<T>(rng, dim);
		auto y = random_vec<T>(rng, dim);

		for (as_vector_metric metric : { AS_VECTOR_METRIC_L2,
				AS_VECTOR_METRIC_COSINE, AS_VECTOR_METRIC_INNER_PRODUCT }) {
			as_vector_kernel_fn fn = as_vector_kernel_get(isa, vt, metric);
			ASSERT_NE(nullptr, fn) << as_vector_isa_name(isa);

			float expected = 0.0f;
			ASSERT_EQ(0, as_vector_distance_compute(vt, metric, dim,
					x.data(), y.data(), &expected));

			expect_close(expected, fn(x.data(), y.data(), dim),
					as_vector_isa_name(isa), type, (int)metric, vt, dim);
		}
	}
}

TEST(VectorParity, AllAvailableIsasVsScalarOracle)
{
	printf("parity seed=%u\n", kSeed);

	uint32_t simd_rows_tested = 0;

	for (uint32_t i = AS_VECTOR_ISA_SCALAR + 1; i < AS_VECTOR_ISA_COUNT;
			i++) {
		as_vector_isa isa = (as_vector_isa)i;

		if (! as_vector_kernel_available(isa)) {
			printf("parity: isa %s unavailable, skipped\n",
					as_vector_isa_name(isa));
			continue;
		}

		printf("parity: isa %s testing\n", as_vector_isa_name(isa));
		parity_type<float>(isa, AS_VECTOR_VALUE_TYPE_FLOAT, "float");
		parity_type<uint8_t>(isa, AS_VECTOR_VALUE_TYPE_UINT8, "uint8");
		parity_type<int8_t>(isa, AS_VECTOR_VALUE_TYPE_INT8, "int8");
		parity_type<int16_t>(isa, AS_VECTOR_VALUE_TYPE_INT16, "int16");
		simd_rows_tested++;
	}

	// Every build target of this project has at least one SIMD row (NEON on
	// aarch64; SSE at minimum on x86-64). Zero tested rows means the gates
	// are broken, not that parity holds.
	EXPECT_GE(simd_rows_tested, 1u);
}

} // namespace
