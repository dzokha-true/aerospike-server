/*
 * vector_distance_sse.cc
 *
 * EC528: SSE distance kernels for VECTOR_DISTANCE (x86-64, runtime-gated on
 * SSE4.1 for the pmovzx/pmovsx widening; universal since 2008).
 *
 * Portions adapted from Microsoft SPTAG DistanceUtils (MIT License): loop
 * structure and distance semantics (float accumulation, L2 sum of squared
 * diffs, cosine family base*base - dot, smaller is better) mirror the
 * MIT-adapted scalar kernels in sptag_distance.h. Lane-parallel accumulation
 * reorders float additions, so parity vs scalar is within relative
 * tolerance, not bit-exact.
 */
#include "vector/vector_kernel.h"

typedef int as_vector_sse_tu_anchor;

#if defined(__x86_64__) || defined(_M_X64)

#include <smmintrin.h> // up to SSE4.1

#include <cstdint>

#include "vector/vector_cpu.h"

namespace {

inline float
reduce4(__m128 v)
{
	__m128 hi = _mm_movehl_ps(v, v);
	v = _mm_add_ps(v, hi);
	__m128 sh = _mm_shuffle_ps(v, v, 0x55);
	v = _mm_add_ss(v, sh);
	return _mm_cvtss_f32(v);
}

// ---------- float32 (4-wide) ----------

float
sse_l2_float(const void* xv, const void* yv, uint32_t dim)
{
	const float* x = (const float*)xv;
	const float* y = (const float*)yv;
	__m128 acc = _mm_setzero_ps();
	uint32_t i = 0;

	for (; i + 4 <= dim; i += 4) {
		__m128 d = _mm_sub_ps(_mm_loadu_ps(x + i), _mm_loadu_ps(y + i));
		acc = _mm_add_ps(acc, _mm_mul_ps(d, d));
	}

	float sum = reduce4(acc);

	for (; i < dim; i++) {
		float d = x[i] - y[i];
		sum += d * d;
	}

	return sum;
}

float
sse_dot_float(const void* xv, const void* yv, uint32_t dim)
{
	const float* x = (const float*)xv;
	const float* y = (const float*)yv;
	__m128 acc = _mm_setzero_ps();
	uint32_t i = 0;

	for (; i + 4 <= dim; i += 4) {
		acc = _mm_add_ps(acc,
				_mm_mul_ps(_mm_loadu_ps(x + i), _mm_loadu_ps(y + i)));
	}

	float sum = reduce4(acc);

	for (; i < dim; i++) {
		sum += x[i] * y[i];
	}

	return sum;
}

// ---------- 8-bit (8-wide; exact int16 diff / int32 madd, float acc) ----------

template<bool IS_SIGNED>
inline __m128i
load8_as_s16(const uint8_t* p)
{
	__m128i b = _mm_loadl_epi64((const __m128i*)p);
	return IS_SIGNED ? _mm_cvtepi8_epi16(b) : _mm_cvtepu8_epi16(b);
}

template<bool IS_SIGNED>
inline float
sse_l2_8bit(const void* xv, const void* yv, uint32_t dim)
{
	const uint8_t* x = (const uint8_t*)xv;
	const uint8_t* y = (const uint8_t*)yv;
	__m128 acc = _mm_setzero_ps();
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		__m128i d = _mm_sub_epi16(load8_as_s16<IS_SIGNED>(x + i),
				load8_as_s16<IS_SIGNED>(y + i));
		// madd(d, d): adjacent squared diffs summed exactly in int32.
		acc = _mm_add_ps(acc, _mm_cvtepi32_ps(_mm_madd_epi16(d, d)));
	}

	float sum = reduce4(acc);

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
sse_dot_8bit(const void* xv, const void* yv, uint32_t dim)
{
	const uint8_t* x = (const uint8_t*)xv;
	const uint8_t* y = (const uint8_t*)yv;
	__m128 acc = _mm_setzero_ps();
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		__m128i p = _mm_madd_epi16(load8_as_s16<IS_SIGNED>(x + i),
				load8_as_s16<IS_SIGNED>(y + i));
		acc = _mm_add_ps(acc, _mm_cvtepi32_ps(p));
	}

	float sum = reduce4(acc);

	for (; i < dim; i++) {
		float p = IS_SIGNED ?
				(float)((const int8_t*)xv)[i] * (float)((const int8_t*)yv)[i] :
				(float)x[i] * (float)y[i];
		sum += p;
	}

	return sum;
}

// ---------- int16 (8-wide) ----------

float
sse_l2_int16(const void* xv, const void* yv, uint32_t dim)
{
	const int16_t* x = (const int16_t*)xv;
	const int16_t* y = (const int16_t*)yv;
	__m128 acc = _mm_setzero_ps();
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		__m128i xs = _mm_loadu_si128((const __m128i*)(x + i));
		__m128i ys = _mm_loadu_si128((const __m128i*)(y + i));

		// Diffs of int16 exceed int16 range: widen to int32 first, square
		// in float lanes (squares can exceed int32).
		__m128i d_lo = _mm_sub_epi32(_mm_cvtepi16_epi32(xs),
				_mm_cvtepi16_epi32(ys));
		__m128i d_hi = _mm_sub_epi32(
				_mm_cvtepi16_epi32(_mm_srli_si128(xs, 8)),
				_mm_cvtepi16_epi32(_mm_srli_si128(ys, 8)));

		__m128 f_lo = _mm_cvtepi32_ps(d_lo);
		__m128 f_hi = _mm_cvtepi32_ps(d_hi);

		acc = _mm_add_ps(acc, _mm_mul_ps(f_lo, f_lo));
		acc = _mm_add_ps(acc, _mm_mul_ps(f_hi, f_hi));
	}

	float sum = reduce4(acc);

	for (; i < dim; i++) {
		float d = (float)x[i] - (float)y[i];
		sum += d * d;
	}

	return sum;
}

float
sse_dot_int16(const void* xv, const void* yv, uint32_t dim)
{
	const int16_t* x = (const int16_t*)xv;
	const int16_t* y = (const int16_t*)yv;
	__m128 acc = _mm_setzero_ps();
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		__m128i xs = _mm_loadu_si128((const __m128i*)(x + i));
		__m128i ys = _mm_loadu_si128((const __m128i*)(y + i));

		// madd: exact int32 pair sums of int16 products.
		acc = _mm_add_ps(acc, _mm_cvtepi32_ps(_mm_madd_epi16(xs, ys)));
	}

	float sum = reduce4(acc);

	for (; i < dim; i++) {
		sum += (float)x[i] * (float)y[i];
	}

	return sum;
}

// ---------- cosine family wrappers ----------

float
sse_cos_float(const void* x, const void* y, uint32_t dim)
{
	return 1.0f - sse_dot_float(x, y, dim);
}

float
sse_cos_uint8(const void* x, const void* y, uint32_t dim)
{
	return (float)(255 * 255) - sse_dot_8bit<false>(x, y, dim);
}

float
sse_cos_int8(const void* x, const void* y, uint32_t dim)
{
	return (float)(127 * 127) - sse_dot_8bit<true>(x, y, dim);
}

float
sse_cos_int16(const void* x, const void* y, uint32_t dim)
{
	return (float)(32767 * 32767) - sse_dot_int16(x, y, dim);
}

float
sse_l2_uint8(const void* x, const void* y, uint32_t dim)
{
	return sse_l2_8bit<false>(x, y, dim);
}

float
sse_l2_int8(const void* x, const void* y, uint32_t dim)
{
	return sse_l2_8bit<true>(x, y, dim);
}

const as_vector_kernel_fn SSE_FNS[AS_VECTOR_KERNEL_TYPE_COUNT][AS_VECTOR_KERNEL_FAMILY_COUNT] = {
	{ sse_l2_float, sse_cos_float },   // AS_VECTOR_VALUE_TYPE_FLOAT
	{ sse_l2_uint8, sse_cos_uint8 },   // AS_VECTOR_VALUE_TYPE_UINT8
	{ sse_l2_int8, sse_cos_int8 },     // AS_VECTOR_VALUE_TYPE_INT8
	{ sse_l2_int16, sse_cos_int16 }    // AS_VECTOR_VALUE_TYPE_INT16
};

__attribute__((constructor))
void
register_sse_kernels(void)
{
	as_vector_kernel_register(AS_VECTOR_ISA_SSE, as_vector_cpu_has_sse41,
			SSE_FNS);
}

} // namespace

#endif // x86-64
