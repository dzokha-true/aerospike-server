/*
 * vector_distance_avx2.cc
 *
 * EC528: AVX2 distance kernels for VECTOR_DISTANCE (x86-64, runtime-gated
 * on OSXSAVE/XCR0 + AVX2).
 *
 * Portions adapted from Microsoft SPTAG DistanceUtils (MIT License): loop
 * structure and distance semantics mirror the MIT-adapted scalar kernels in
 * sptag_distance.h. Lane-parallel accumulation reorders float additions, so
 * parity vs scalar is within relative tolerance, not bit-exact.
 */
#include "vector/vector_kernel.h"

typedef int as_vector_avx2_tu_anchor;

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>

#include <cstdint>

#include "vector/vector_cpu.h"

namespace {

inline float
reduce8(__m256 v)
{
	__m128 lo = _mm256_castps256_ps128(v);
	__m128 hi = _mm256_extractf128_ps(v, 1);
	__m128 s = _mm_add_ps(lo, hi);
	__m128 mh = _mm_movehl_ps(s, s);
	s = _mm_add_ps(s, mh);
	__m128 sh = _mm_shuffle_ps(s, s, 0x55);
	s = _mm_add_ss(s, sh);
	return _mm_cvtss_f32(s);
}

// ---------- float32 (8-wide) ----------

float
avx2_l2_float(const void* xv, const void* yv, uint32_t dim)
{
	const float* x = (const float*)xv;
	const float* y = (const float*)yv;
	__m256 acc = _mm256_setzero_ps();
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		__m256 d = _mm256_sub_ps(_mm256_loadu_ps(x + i),
				_mm256_loadu_ps(y + i));
		acc = _mm256_add_ps(acc, _mm256_mul_ps(d, d));
	}

	float sum = reduce8(acc);

	for (; i < dim; i++) {
		float d = x[i] - y[i];
		sum += d * d;
	}

	return sum;
}

float
avx2_dot_float(const void* xv, const void* yv, uint32_t dim)
{
	const float* x = (const float*)xv;
	const float* y = (const float*)yv;
	__m256 acc = _mm256_setzero_ps();
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		acc = _mm256_add_ps(acc, _mm256_mul_ps(_mm256_loadu_ps(x + i),
				_mm256_loadu_ps(y + i)));
	}

	float sum = reduce8(acc);

	for (; i < dim; i++) {
		sum += x[i] * y[i];
	}

	return sum;
}

// ---------- 8-bit (16-wide) ----------

template<bool IS_SIGNED>
inline __m256i
load16_as_s16(const uint8_t* p)
{
	__m128i b = _mm_loadu_si128((const __m128i*)p);
	return IS_SIGNED ? _mm256_cvtepi8_epi16(b) : _mm256_cvtepu8_epi16(b);
}

template<bool IS_SIGNED>
inline float
avx2_l2_8bit(const void* xv, const void* yv, uint32_t dim)
{
	const uint8_t* x = (const uint8_t*)xv;
	const uint8_t* y = (const uint8_t*)yv;
	__m256 acc = _mm256_setzero_ps();
	uint32_t i = 0;

	for (; i + 16 <= dim; i += 16) {
		__m256i d = _mm256_sub_epi16(load16_as_s16<IS_SIGNED>(x + i),
				load16_as_s16<IS_SIGNED>(y + i));
		acc = _mm256_add_ps(acc,
				_mm256_cvtepi32_ps(_mm256_madd_epi16(d, d)));
	}

	float sum = reduce8(acc);

	for (; i < dim; i++) {
		float d = IS_SIGNED ?
				(float)((const int8_t*)xv)[i] - (float)((const int8_t*)yv)[i] :
				(float)x[i] - (float)y[i];
		sum += d * d;
	}

	return sum;
}

template<bool IS_SIGNED>
inline float
avx2_dot_8bit(const void* xv, const void* yv, uint32_t dim)
{
	const uint8_t* x = (const uint8_t*)xv;
	const uint8_t* y = (const uint8_t*)yv;
	__m256 acc = _mm256_setzero_ps();
	uint32_t i = 0;

	for (; i + 16 <= dim; i += 16) {
		__m256i p = _mm256_madd_epi16(load16_as_s16<IS_SIGNED>(x + i),
				load16_as_s16<IS_SIGNED>(y + i));
		acc = _mm256_add_ps(acc, _mm256_cvtepi32_ps(p));
	}

	float sum = reduce8(acc);

	for (; i < dim; i++) {
		float p = IS_SIGNED ?
				(float)((const int8_t*)xv)[i] * (float)((const int8_t*)yv)[i] :
				(float)x[i] * (float)y[i];
		sum += p;
	}

	return sum;
}

// ---------- int16 (16-wide) ----------

float
avx2_l2_int16(const void* xv, const void* yv, uint32_t dim)
{
	const int16_t* x = (const int16_t*)xv;
	const int16_t* y = (const int16_t*)yv;
	__m256 acc = _mm256_setzero_ps();
	uint32_t i = 0;

	for (; i + 16 <= dim; i += 16) {
		__m256i xs = _mm256_loadu_si256((const __m256i*)(x + i));
		__m256i ys = _mm256_loadu_si256((const __m256i*)(y + i));

		__m256i d_lo = _mm256_sub_epi32(
				_mm256_cvtepi16_epi32(_mm256_castsi256_si128(xs)),
				_mm256_cvtepi16_epi32(_mm256_castsi256_si128(ys)));
		__m256i d_hi = _mm256_sub_epi32(
				_mm256_cvtepi16_epi32(_mm256_extracti128_si256(xs, 1)),
				_mm256_cvtepi16_epi32(_mm256_extracti128_si256(ys, 1)));

		__m256 f_lo = _mm256_cvtepi32_ps(d_lo);
		__m256 f_hi = _mm256_cvtepi32_ps(d_hi);

		acc = _mm256_add_ps(acc, _mm256_mul_ps(f_lo, f_lo));
		acc = _mm256_add_ps(acc, _mm256_mul_ps(f_hi, f_hi));
	}

	float sum = reduce8(acc);

	for (; i < dim; i++) {
		float d = (float)x[i] - (float)y[i];
		sum += d * d;
	}

	return sum;
}

float
avx2_dot_int16(const void* xv, const void* yv, uint32_t dim)
{
	const int16_t* x = (const int16_t*)xv;
	const int16_t* y = (const int16_t*)yv;
	__m256 acc = _mm256_setzero_ps();
	uint32_t i = 0;

	for (; i + 16 <= dim; i += 16) {
		__m256i xs = _mm256_loadu_si256((const __m256i*)(x + i));
		__m256i ys = _mm256_loadu_si256((const __m256i*)(y + i));

		acc = _mm256_add_ps(acc,
				_mm256_cvtepi32_ps(_mm256_madd_epi16(xs, ys)));
	}

	float sum = reduce8(acc);

	for (; i < dim; i++) {
		sum += (float)x[i] * (float)y[i];
	}

	return sum;
}

// ---------- cosine family wrappers ----------

float
avx2_cos_float(const void* x, const void* y, uint32_t dim)
{
	return 1.0f - avx2_dot_float(x, y, dim);
}

float
avx2_cos_uint8(const void* x, const void* y, uint32_t dim)
{
	return (float)(255 * 255) - avx2_dot_8bit<false>(x, y, dim);
}

float
avx2_cos_int8(const void* x, const void* y, uint32_t dim)
{
	return (float)(127 * 127) - avx2_dot_8bit<true>(x, y, dim);
}

float
avx2_cos_int16(const void* x, const void* y, uint32_t dim)
{
	return (float)(32767 * 32767) - avx2_dot_int16(x, y, dim);
}

float
avx2_l2_uint8(const void* x, const void* y, uint32_t dim)
{
	return avx2_l2_8bit<false>(x, y, dim);
}

float
avx2_l2_int8(const void* x, const void* y, uint32_t dim)
{
	return avx2_l2_8bit<true>(x, y, dim);
}

const as_vector_kernel_fn AVX2_FNS[AS_VECTOR_KERNEL_TYPE_COUNT][AS_VECTOR_KERNEL_FAMILY_COUNT] = {
	{ avx2_l2_float, avx2_cos_float },   // FLOAT
	{ avx2_l2_uint8, avx2_cos_uint8 },   // UINT8
	{ avx2_l2_int8, avx2_cos_int8 },     // INT8
	{ avx2_l2_int16, avx2_cos_int16 }    // INT16
};

__attribute__((constructor))
void
register_avx2_kernels(void)
{
	as_vector_kernel_register(AS_VECTOR_ISA_AVX2, as_vector_cpu_has_avx2,
			AVX2_FNS);
}

} // namespace

#endif // x86-64
