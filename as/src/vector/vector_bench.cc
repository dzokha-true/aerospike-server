/*
 * vector_bench.cc
 *
 * EC528: kernel microbenchmark - ns per tail-vector distance for each
 * runtime-available ISA row vs scalar. Markdown table to stdout. Not a
 * correctness tool; parity is proven by the gtest suites.
 */
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "vector/vector_kernel.h"

namespace {

constexpr uint32_t kSeed = 20260715;

template<typename T>
std::vector<T>
random_vec(std::mt19937& rng, uint32_t n)
{
	std::vector<T> v(n);

	if (std::is_same<T, float>::value) {
		std::uniform_real_distribution<float> d(-1.0f, 1.0f);
		for (auto& e : v) {
			e = (T)d(rng);
		}
	}
	else {
		std::uniform_int_distribution<int32_t> d(-100, 100);
		for (auto& e : v) {
			e = (T)d(rng);
		}
	}

	return v;
}

// Volatile sink so the loop is not optimized away.
volatile float g_sink;

template<typename T>
double
bench_one(as_vector_kernel_fn fn, uint32_t dim)
{
	std::mt19937 rng(kSeed);

	// A pool of tail vectors large enough to defeat L1 residency games but
	// small enough to stay in L2: mimics scanning a posting list.
	const uint32_t pool = 256;
	auto query = random_vec<T>(rng, dim);
	auto tails = random_vec<T>(rng, dim * pool);

	// Warmup + calibrated iteration count.
	uint32_t iters = 200000000u / (dim * 4u) + 1000u;

	for (uint32_t i = 0; i < 1000; i++) {
		g_sink = fn(query.data(), tails.data() + (i % pool) * dim, dim);
	}

	auto t0 = std::chrono::steady_clock::now();

	for (uint32_t i = 0; i < iters; i++) {
		g_sink = fn(query.data(), tails.data() + (i % pool) * dim, dim);
	}

	auto t1 = std::chrono::steady_clock::now();
	double ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
			t1 - t0).count();

	return ns / iters;
}

template<typename T>
void
bench_type(as_vector_value_type vt, const char* type)
{
	const uint32_t dims[] = { 128, 768, 1024 };

	for (uint32_t dim : dims) {
		for (as_vector_metric metric : { AS_VECTOR_METRIC_L2,
				AS_VECTOR_METRIC_COSINE }) {
			const char* mname = metric == AS_VECTOR_METRIC_L2 ? "l2" :
					"cosine";
			double scalar_ns = 0.0;

			for (uint32_t i = 0; i < AS_VECTOR_ISA_COUNT; i++) {
				as_vector_isa isa = (as_vector_isa)i;

				if (! as_vector_kernel_available(isa)) {
					continue;
				}

				as_vector_kernel_fn fn = as_vector_kernel_get(isa, vt,
						metric);

				if (fn == nullptr) {
					continue;
				}

				double ns = bench_one<T>(fn, dim);

				if (isa == AS_VECTOR_ISA_SCALAR) {
					scalar_ns = ns;
				}

				printf("| %s | %s | %u | %s | %.1f | %.2fx |\n", type, mname,
						dim, as_vector_isa_name(isa), ns,
						scalar_ns > 0.0 ? scalar_ns / ns : 1.0);
			}
		}
	}
}

} // namespace

int
main(void)
{
	printf("# vector kernel microbenchmark (ns/vector; speedup vs scalar)\n\n");
	printf("| type | metric | dim | isa | ns/vector | speedup |\n");
	printf("|---|---|---|---|---|---|\n");

	bench_type<float>(AS_VECTOR_VALUE_TYPE_FLOAT, "float");
	bench_type<int8_t>(AS_VECTOR_VALUE_TYPE_INT8, "int8");

	return 0;
}
