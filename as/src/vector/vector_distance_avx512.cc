/*
 * vector_distance_avx512.cc
 *
 * EC528: AVX-512 distance kernels for VECTOR_DISTANCE (x86-64, runtime-gated
 * on OSXSAVE/XCR0 zmm + AVX512F + AVX512BW - the integer kernels need BW).
 *
 * Portions adapted from Microsoft SPTAG DistanceUtils (MIT License): loop
 * structure and distance semantics mirror the MIT-adapted scalar kernels in
 * sptag_distance.h. Lane-parallel accumulation reorders float additions, so
 * parity vs scalar is within relative tolerance, not bit-exact.
 */
#include "vector/vector_kernel.h"

typedef int as_vector_avx512_tu_anchor;

#if defined(__x86_64__) || defined(_M_X64)

#include <immintrin.h>

#include <cstdint>

#include "vector/vector_cpu.h"

namespace {

// ---------- float32 (16-wide) ----------

float
avx512_l2_float(const void* xv, const void* yv, uint32_t dim)
{
	const float* x = (const float*)xv;
	const float* y = (const float*)yv;
	__m512 acc = _mm512_setzero_ps();
	uint32_t i = 0;

	for (; i + 16 <= dim; i += 16) {
		__m512 d = _mm512_sub_ps(_mm512_loadu_ps(x + i),
				_mm512_loadu_ps(y + i));
		acc = _mm512_add_ps(acc, _mm512_mul_ps(d, d));
	}

	float sum = _mm512_reduce_add_ps(acc);

	for (; i < dim; i++) {
		float d = x[i] - y[i];
		sum += d * d;
	}

	return sum;
}

float
avx512_dot_float(const void* xv, const void* yv, uint32_t dim)
{
	const float* x = (const float*)xv;
	const float* y = (const float*)yv;
	__m512 acc = _mm512_setzero_ps();
	uint32_t i = 0;

	for (; i + 16 <= dim; i += 16) {
		acc = _mm512_add_ps(acc, _mm512_mul_ps(_mm512_loadu_ps(x + i),
				_mm512_loadu_ps(y + i)));
	}

	float sum = _mm512_reduce_add_ps(acc);

	for (; i < dim; i++) {
		sum += x[i] * y[i];
	}

	return sum;
}

// ---------- 8-bit (32-wide; needs AVX512BW) ----------

template<bool IS_SIGNED>
inline __m512i
load32_as_s16(const uint8_t* p)
{
	__m256i b = _mm256_loadu_si256((const __m256i*)p);
	return IS_SIGNED ? _mm512_cvtepi8_epi16(b) : _mm512_cvtepu8_epi16(b);
}

template<bool IS_SIGNED>
inline float
avx512_l2_8bit(const void* xv, const void* yv, uint32_t dim)
{
	const uint8_t* x = (const uint8_t*)xv;
	const uint8_t* y = (const uint8_t*)yv;
	__m512 acc = _mm512_setzero_ps();
	uint32_t i = 0;

	for (; i + 32 <= dim; i += 32) {
		__m512i d = _mm512_sub_epi16(load32_as_s16<IS_SIGNED>(x + i),
				load32_as_s16<IS_SIGNED>(y + i));
		acc = _mm512_add_ps(acc,
				_mm512_cvtepi32_ps(_mm512_madd_epi16(d, d)));
	}

	float sum = _mm512_reduce_add_ps(acc);

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
avx512_dot_8bit(const void* xv, const void* yv, uint32_t dim)
{
	const uint8_t* x = (const uint8_t*)xv;
	const uint8_t* y = (const uint8_t*)yv;
	__m512 acc = _mm512_setzero_ps();
	uint32_t i = 0;

	for (; i + 32 <= dim; i += 32) {
		__m512i p = _mm512_madd_epi16(load32_as_s16<IS_SIGNED>(x + i),
				load32_as_s16<IS_SIGNED>(y + i));
		acc = _mm512_add_ps(acc, _mm512_cvtepi32_ps(p));
	}

	float sum = _mm512_reduce_add_ps(acc);

	for (; i < dim; i++) {
		float p = IS_SIGNED ?
				(float)((const int8_t*)xv)[i] * (float)((const int8_t*)yv)[i] :
				(float)x[i] * (float)y[i];
		sum += p;
	}

	return sum;
}

// ---------- int16 (32-wide; needs AVX512BW) ----------

float
avx512_l2_int16(const void* xv, const void* yv, uint32_t dim)
{
	const int16_t* x = (const int16_t*)xv;
	const int16_t* y = (const int16_t*)yv;
	__m512 acc = _mm512_setzero_ps();
	uint32_t i = 0;

	for (; i + 32 <= dim; i += 32) {
		__m512i xs = _mm512_loadu_si512((const void*)(x + i));
		__m512i ys = _mm512_loadu_si512((const void*)(y + i));

		__m512i d_lo = _mm512_sub_epi32(
				_mm512_cvtepi16_epi32(_mm512_castsi512_si256(xs)),
				_mm512_cvtepi16_epi32(_mm512_castsi512_si256(ys)));
		__m512i d_hi = _mm512_sub_epi32(
				_mm512_cvtepi16_epi32(_mm512_extracti64x4_epi64(xs, 1)),
				_mm512_cvtepi16_epi32(_mm512_extracti64x4_epi64(ys, 1)));

		__m512 f_lo = _mm512_cvtepi32_ps(d_lo);
		__m512 f_hi = _mm512_cvtepi32_ps(d_hi);

		acc = _mm512_add_ps(acc, _mm512_mul_ps(f_lo, f_lo));
		acc = _mm512_add_ps(acc, _mm512_mul_ps(f_hi, f_hi));
	}

	float sum = _mm512_reduce_add_ps(acc);

	for (; i < dim; i++) {
		float d = (float)x[i] - (float)y[i];
		sum += d * d;
	}

	return sum;
}

float
avx512_dot_int16(const void* xv, const void* yv, uint32_t dim)
{
	const int16_t* x = (const int16_t*)xv;
	const int16_t* y = (const int16_t*)yv;
	__m512 acc = _mm512_setzero_ps();
	uint32_t i = 0;

	for (; i + 32 <= dim; i += 32) {
		__m512i xs = _mm512_loadu_si512((const void*)(x + i));
		__m512i ys = _mm512_loadu_si512((const void*)(y + i));

		acc = _mm512_add_ps(acc,
				_mm512_cvtepi32_ps(_mm512_madd_epi16(xs, ys)));
	}

	float sum = _mm512_reduce_add_ps(acc);

	for (; i < dim; i++) {
		sum += (float)x[i] * (float)y[i];
	}

	return sum;
}

// ---------- cosine family wrappers ----------

float
avx512_cos_float(const void* x, const void* y, uint32_t dim)
{
	return 1.0f - avx512_dot_float(x, y, dim);
}

float
avx512_cos_uint8(const void* x, const void* y, uint32_t dim)
{
	return (float)(255 * 255) - avx512_dot_8bit<false>(x, y, dim);
}

float
avx512_cos_int8(const void* x, const void* y, uint32_t dim)
{
	return (float)(127 * 127) - avx512_dot_8bit<true>(x, y, dim);
}

float
avx512_cos_int16(const void* x, const void* y, uint32_t dim)
{
	return (float)(32767 * 32767) - avx512_dot_int16(x, y, dim);
}

float
avx512_l2_uint8(const void* x, const void* y, uint32_t dim)
{
	return avx512_l2_8bit<false>(x, y, dim);
}

float
avx512_l2_int8(const void* x, const void* y, uint32_t dim)
{
	return avx512_l2_8bit<true>(x, y, dim);
}

const as_vector_kernel_fn AVX512_FNS[AS_VECTOR_KERNEL_TYPE_COUNT][AS_VECTOR_KERNEL_FAMILY_COUNT] = {
	{ avx512_l2_float, avx512_cos_float },   // FLOAT
	{ avx512_l2_uint8, avx512_cos_uint8 },   // UINT8
	{ avx512_l2_int8, avx512_cos_int8 },     // INT8
	{ avx512_l2_int16, avx512_cos_int16 }    // INT16
};

__attribute__((constructor))
void
register_avx512_kernels(void)
{
	as_vector_kernel_register(AS_VECTOR_ISA_AVX512,
			as_vector_cpu_has_avx512bw, AVX512_FNS);
}

} // namespace

#endif // x86-64
