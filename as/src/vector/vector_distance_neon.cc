/*
 * vector_distance_neon.cc
 *
 * EC528: NEON distance kernels for VECTOR_DISTANCE (aarch64 baseline - every
 * armv8-a core has ASIMD, so there is no runtime feature probe).
 *
 * Semantics mirror the scalar kernels in sptag_distance.h: all accumulation
 * happens in float, L2 is sum((x-y)^2), the cosine family returns
 * base*base - dot (smaller is better). Lane-parallel float accumulation
 * reorders the additions relative to the sequential scalar loop, so results
 * match the scalar oracle within relative tolerance, not bit-for-bit; the
 * parity tests document and enforce that bound.
 */
#include "vector/vector_kernel.h"

// Keep the TU non-empty on non-aarch64 builds.
typedef int as_vector_neon_tu_anchor;

#if defined(__aarch64__)

#include <arm_neon.h>

#include <cstdint>

namespace {

inline float
reduce4(float32x4_t acc)
{
	return vaddvq_f32(acc);
}

// ---------- float32 ----------

float
neon_l2_float(const void* xv, const void* yv, uint32_t dim)
{
	const float* x = (const float*)xv;
	const float* y = (const float*)yv;
	float32x4_t acc = vdupq_n_f32(0.0f);
	uint32_t i = 0;

	for (; i + 4 <= dim; i += 4) {
		float32x4_t d = vsubq_f32(vld1q_f32(x + i), vld1q_f32(y + i));
		acc = vfmaq_f32(acc, d, d);
	}

	float sum = reduce4(acc);

	for (; i < dim; i++) {
		float d = x[i] - y[i];
		sum += d * d;
	}

	return sum;
}

float
neon_dot_float(const void* xv, const void* yv, uint32_t dim)
{
	const float* x = (const float*)xv;
	const float* y = (const float*)yv;
	float32x4_t acc = vdupq_n_f32(0.0f);
	uint32_t i = 0;

	for (; i + 4 <= dim; i += 4) {
		acc = vfmaq_f32(acc, vld1q_f32(x + i), vld1q_f32(y + i));
	}

	float sum = reduce4(acc);

	for (; i < dim; i++) {
		sum += x[i] * y[i];
	}

	return sum;
}

// ---------- 8-bit helpers ----------
// Widen both operands to signed 16 (uint8 values fit), subtract or multiply
// exactly in integer lanes (|diff| <= 255 so diff^2 <= 65025; products of
// widened 8-bit values fit int32), then accumulate in float lanes.

template<bool IS_SIGNED>
inline void
load8_as_s16(const uint8_t* p, int16x8_t* out)
{
	if (IS_SIGNED) {
		*out = vmovl_s8(vld1_s8((const int8_t*)p));
	}
	else {
		*out = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(p)));
	}
}

template<bool IS_SIGNED>
inline float
neon_l2_8bit(const void* xv, const void* yv, uint32_t dim)
{
	const uint8_t* x = (const uint8_t*)xv;
	const uint8_t* y = (const uint8_t*)yv;
	float32x4_t acc = vdupq_n_f32(0.0f);
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		int16x8_t xs, ys;
		load8_as_s16<IS_SIGNED>(x + i, &xs);
		load8_as_s16<IS_SIGNED>(y + i, &ys);

		int16x8_t d = vsubq_s16(xs, ys);
		int32x4_t sq_lo = vmull_s16(vget_low_s16(d), vget_low_s16(d));
		int32x4_t sq_hi = vmull_s16(vget_high_s16(d), vget_high_s16(d));

		acc = vaddq_f32(acc, vcvtq_f32_s32(sq_lo));
		acc = vaddq_f32(acc, vcvtq_f32_s32(sq_hi));
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
neon_dot_8bit(const void* xv, const void* yv, uint32_t dim)
{
	const uint8_t* x = (const uint8_t*)xv;
	const uint8_t* y = (const uint8_t*)yv;
	float32x4_t acc = vdupq_n_f32(0.0f);
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		int16x8_t xs, ys;
		load8_as_s16<IS_SIGNED>(x + i, &xs);
		load8_as_s16<IS_SIGNED>(y + i, &ys);

		int32x4_t p_lo = vmull_s16(vget_low_s16(xs), vget_low_s16(ys));
		int32x4_t p_hi = vmull_s16(vget_high_s16(xs), vget_high_s16(ys));

		acc = vaddq_f32(acc, vcvtq_f32_s32(p_lo));
		acc = vaddq_f32(acc, vcvtq_f32_s32(p_hi));
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

// ---------- int16 ----------
// L2 diffs fit int32 but their squares can overflow int32, so square in
// float lanes; dot products of int16 pairs fit int32 exactly.

inline float
neon_l2_int16(const void* xv, const void* yv, uint32_t dim)
{
	const int16_t* x = (const int16_t*)xv;
	const int16_t* y = (const int16_t*)yv;
	float32x4_t acc = vdupq_n_f32(0.0f);
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		int16x8_t xs = vld1q_s16(x + i);
		int16x8_t ys = vld1q_s16(y + i);

		int32x4_t d_lo = vsubl_s16(vget_low_s16(xs), vget_low_s16(ys));
		int32x4_t d_hi = vsubl_s16(vget_high_s16(xs), vget_high_s16(ys));

		float32x4_t f_lo = vcvtq_f32_s32(d_lo);
		float32x4_t f_hi = vcvtq_f32_s32(d_hi);

		acc = vfmaq_f32(acc, f_lo, f_lo);
		acc = vfmaq_f32(acc, f_hi, f_hi);
	}

	float sum = reduce4(acc);

	for (; i < dim; i++) {
		float d = (float)x[i] - (float)y[i];
		sum += d * d;
	}

	return sum;
}

inline float
neon_dot_int16(const void* xv, const void* yv, uint32_t dim)
{
	const int16_t* x = (const int16_t*)xv;
	const int16_t* y = (const int16_t*)yv;
	float32x4_t acc = vdupq_n_f32(0.0f);
	uint32_t i = 0;

	for (; i + 8 <= dim; i += 8) {
		int16x8_t xs = vld1q_s16(x + i);
		int16x8_t ys = vld1q_s16(y + i);

		int32x4_t p_lo = vmull_s16(vget_low_s16(xs), vget_low_s16(ys));
		int32x4_t p_hi = vmull_s16(vget_high_s16(xs), vget_high_s16(ys));

		acc = vaddq_f32(acc, vcvtq_f32_s32(p_lo));
		acc = vaddq_f32(acc, vcvtq_f32_s32(p_hi));
	}

	float sum = reduce4(acc);

	for (; i < dim; i++) {
		sum += (float)x[i] * (float)y[i];
	}

	return sum;
}

// ---------- cosine family wrappers (base*base - dot) ----------

float
neon_cos_float(const void* x, const void* y, uint32_t dim)
{
	return 1.0f - neon_dot_float(x, y, dim);
}

float
neon_cos_uint8(const void* x, const void* y, uint32_t dim)
{
	return (float)(255 * 255) - neon_dot_8bit<false>(x, y, dim);
}

float
neon_cos_int8(const void* x, const void* y, uint32_t dim)
{
	return (float)(127 * 127) - neon_dot_8bit<true>(x, y, dim);
}

float
neon_cos_int16(const void* x, const void* y, uint32_t dim)
{
	return (float)(32767 * 32767) - neon_dot_int16(x, y, dim);
}

float
neon_l2_uint8(const void* x, const void* y, uint32_t dim)
{
	return neon_l2_8bit<false>(x, y, dim);
}

float
neon_l2_int8(const void* x, const void* y, uint32_t dim)
{
	return neon_l2_8bit<true>(x, y, dim);
}

const as_vector_kernel_fn NEON_FNS[AS_VECTOR_KERNEL_TYPE_COUNT][AS_VECTOR_KERNEL_FAMILY_COUNT] = {
	[AS_VECTOR_VALUE_TYPE_FLOAT] = { neon_l2_float, neon_cos_float },
	[AS_VECTOR_VALUE_TYPE_UINT8] = { neon_l2_uint8, neon_cos_uint8 },
	[AS_VECTOR_VALUE_TYPE_INT8] = { neon_l2_int8, neon_cos_int8 },
	[AS_VECTOR_VALUE_TYPE_INT16] = { neon_l2_int16, neon_cos_int16 }
};

__attribute__((constructor))
void
register_neon_kernels(void)
{
	// armv8-a guarantees ASIMD: no runtime predicate needed.
	as_vector_kernel_register(AS_VECTOR_ISA_NEON, nullptr, NEON_FNS);
}

} // namespace

#endif // __aarch64__
