/*
 * vector_types.c
 *
 * EC528: SPTAG vector distance shared types.
 */
#include "vector/vector_types.h"

#include <string.h>

uint32_t
as_vector_value_type_size(as_vector_value_type type)
{
	switch (type) {
	case AS_VECTOR_VALUE_TYPE_FLOAT:
		return 4;
	case AS_VECTOR_VALUE_TYPE_UINT8:
	case AS_VECTOR_VALUE_TYPE_INT8:
		return 1;
	case AS_VECTOR_VALUE_TYPE_INT16:
		return 2;
	default:
		return 0;
	}
}

bool
as_vector_value_type_from_string(const char* s, as_vector_value_type* out)
{
	if (strcmp(s, "float") == 0) {
		*out = AS_VECTOR_VALUE_TYPE_FLOAT;
		return true;
	}
	if (strcmp(s, "uint8") == 0) {
		*out = AS_VECTOR_VALUE_TYPE_UINT8;
		return true;
	}
	if (strcmp(s, "int8") == 0) {
		*out = AS_VECTOR_VALUE_TYPE_INT8;
		return true;
	}
	if (strcmp(s, "int16") == 0) {
		*out = AS_VECTOR_VALUE_TYPE_INT16;
		return true;
	}
	return false;
}

bool
as_vector_metric_from_string(const char* s, as_vector_metric* out)
{
	if (strcmp(s, "l2") == 0) {
		*out = AS_VECTOR_METRIC_L2;
		return true;
	}
	if (strcmp(s, "cosine") == 0) {
		*out = AS_VECTOR_METRIC_COSINE;
		return true;
	}
	if (strcmp(s, "inner-product") == 0) {
		*out = AS_VECTOR_METRIC_INNER_PRODUCT;
		return true;
	}
	return false;
}

bool
as_vector_namespace_cfg_valid(const as_vector_namespace_cfg* ns)
{
	if (ns->vector_dimension == 0) {
		return false;
	}
	if ((as_vector_value_type)ns->vector_value_type == AS_VECTOR_VALUE_TYPE_BAD) {
		return false;
	}
	if ((as_vector_metric)ns->vector_metric == AS_VECTOR_METRIC_BAD) {
		return false;
	}
	uint32_t query_bytes =
			ns->vector_dimension *
			as_vector_value_type_size((as_vector_value_type)ns->vector_value_type);
	if (ns->vector_max_query_bytes != 0 && query_bytes > ns->vector_max_query_bytes) {
		return false;
	}
	return true;
}
