/*
 * vector_kernel.c
 *
 * EC528: runtime kernel dispatch table for VECTOR_DISTANCE tail scoring.
 */
#include "vector/vector_kernel.h"

#include <stddef.h>
#include <string.h>

// Scalar kernels live in vector_distance.cc (C linkage wrappers around the
// MIT-adapted SPTAG templates).
extern float as_vector_scalar_l2_float(const void* x, const void* y, uint32_t dim);
extern float as_vector_scalar_cos_float(const void* x, const void* y, uint32_t dim);
extern float as_vector_scalar_l2_uint8(const void* x, const void* y, uint32_t dim);
extern float as_vector_scalar_cos_uint8(const void* x, const void* y, uint32_t dim);
extern float as_vector_scalar_l2_int8(const void* x, const void* y, uint32_t dim);
extern float as_vector_scalar_cos_int8(const void* x, const void* y, uint32_t dim);
extern float as_vector_scalar_l2_int16(const void* x, const void* y, uint32_t dim);
extern float as_vector_scalar_cos_int16(const void* x, const void* y, uint32_t dim);

typedef struct kernel_row_s {
	bool registered;
	as_vector_isa_supported_fn supported; // NULL = always supported
	as_vector_kernel_fn fns[AS_VECTOR_KERNEL_TYPE_COUNT][AS_VECTOR_KERNEL_FAMILY_COUNT];
} kernel_row;

static kernel_row g_rows[AS_VECTOR_ISA_COUNT] = {
	[AS_VECTOR_ISA_SCALAR] = {
		.registered = true,
		.supported = NULL,
		.fns = {
			[AS_VECTOR_VALUE_TYPE_FLOAT] = {
				[AS_VECTOR_KERNEL_FAMILY_L2] = as_vector_scalar_l2_float,
				[AS_VECTOR_KERNEL_FAMILY_COSINE] = as_vector_scalar_cos_float
			},
			[AS_VECTOR_VALUE_TYPE_UINT8] = {
				[AS_VECTOR_KERNEL_FAMILY_L2] = as_vector_scalar_l2_uint8,
				[AS_VECTOR_KERNEL_FAMILY_COSINE] = as_vector_scalar_cos_uint8
			},
			[AS_VECTOR_VALUE_TYPE_INT8] = {
				[AS_VECTOR_KERNEL_FAMILY_L2] = as_vector_scalar_l2_int8,
				[AS_VECTOR_KERNEL_FAMILY_COSINE] = as_vector_scalar_cos_int8
			},
			[AS_VECTOR_VALUE_TYPE_INT16] = {
				[AS_VECTOR_KERNEL_FAMILY_L2] = as_vector_scalar_l2_int16,
				[AS_VECTOR_KERNEL_FAMILY_COSINE] = as_vector_scalar_cos_int16
			}
		}
	}
};

static const char* const g_isa_names[AS_VECTOR_ISA_COUNT] = {
	"scalar", "sse", "avx2", "avx512", "neon"
};

bool
as_vector_kernel_available(as_vector_isa isa)
{
	if ((uint32_t)isa >= AS_VECTOR_ISA_COUNT) {
		return false;
	}

	const kernel_row* row = &g_rows[isa];

	if (! row->registered) {
		return false;
	}

	return row->supported == NULL || row->supported();
}

int
as_vector_kernel_choose(const char* mode, as_vector_isa* out)
{
	if (mode == NULL || mode[0] == '\0' || strcmp(mode, "auto") == 0) {
		// Best available: highest enum value. Cross-arch rows never
		// co-register, so enum order is a total preference order.
		for (int32_t i = AS_VECTOR_ISA_COUNT - 1; i >= 0; i--) {
			if (as_vector_kernel_available((as_vector_isa)i)) {
				*out = (as_vector_isa)i;
				return 0;
			}
		}
		// SCALAR is statically registered - unreachable, but stay loud.
		return -2;
	}

	for (uint32_t i = 0; i < AS_VECTOR_ISA_COUNT; i++) {
		if (strcmp(mode, g_isa_names[i]) == 0) {
			if (! as_vector_kernel_available((as_vector_isa)i)) {
				return -2;
			}
			*out = (as_vector_isa)i;
			return 0;
		}
	}

	return -1;
}

as_vector_kernel_fn
as_vector_kernel_get(as_vector_isa isa, as_vector_value_type value_type,
		as_vector_metric metric)
{
	if ((uint32_t)isa >= AS_VECTOR_ISA_COUNT ||
			(uint32_t)value_type >= AS_VECTOR_KERNEL_TYPE_COUNT) {
		return NULL;
	}

	as_vector_kernel_family family;

	switch (metric) {
	case AS_VECTOR_METRIC_L2:
		family = AS_VECTOR_KERNEL_FAMILY_L2;
		break;
	case AS_VECTOR_METRIC_COSINE:
	case AS_VECTOR_METRIC_INNER_PRODUCT:
		family = AS_VECTOR_KERNEL_FAMILY_COSINE;
		break;
	default:
		return NULL;
	}

	const kernel_row* row = &g_rows[isa];

	if (! row->registered) {
		return NULL;
	}

	return row->fns[value_type][family];
}

void
as_vector_kernel_register(as_vector_isa isa,
		as_vector_isa_supported_fn supported,
		const as_vector_kernel_fn
				fns[AS_VECTOR_KERNEL_TYPE_COUNT][AS_VECTOR_KERNEL_FAMILY_COUNT])
{
	if ((uint32_t)isa >= AS_VECTOR_ISA_COUNT || isa == AS_VECTOR_ISA_SCALAR ||
			fns == NULL) {
		return; // scalar row is immutable; bogus registrations are ignored
	}

	kernel_row* row = &g_rows[isa];

	memcpy(row->fns, fns, sizeof(row->fns));
	row->supported = supported;
	row->registered = true;
}

const char*
as_vector_isa_name(as_vector_isa isa)
{
	if ((uint32_t)isa >= AS_VECTOR_ISA_COUNT) {
		return "invalid";
	}

	return g_isa_names[isa];
}
