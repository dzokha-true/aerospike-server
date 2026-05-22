/*
 * vector_types.h
 *
 * EC528: SPTAG vector distance shared types.
 */
#pragma once

#include <stdint.h>

#include "vector/vector_namespace_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AS_VECTOR_WIRE_VERSION 1

#define AS_VECTOR_HEAD_ID_MAX INT32_MAX

// EC528: bound dimension so dimension * sizeof(value_type) fits uint32 with
// room to spare; far above any realistic SPTAG ANN dimension.
#define AS_VECTOR_MAX_DIMENSION (1u << 16)

typedef enum {
	AS_VECTOR_VALUE_TYPE_FLOAT = 0,
	AS_VECTOR_VALUE_TYPE_UINT8 = 1,
	AS_VECTOR_VALUE_TYPE_INT8 = 2,
	AS_VECTOR_VALUE_TYPE_INT16 = 3,
	AS_VECTOR_VALUE_TYPE_BAD = 255
} as_vector_value_type;

typedef enum {
	AS_VECTOR_METRIC_L2 = 0,
	AS_VECTOR_METRIC_COSINE = 1,
	AS_VECTOR_METRIC_INNER_PRODUCT = 2,
	AS_VECTOR_METRIC_BAD = 255
} as_vector_metric;

typedef enum {
	AS_VECTOR_REQ_OK = 0,
	AS_VECTOR_REQ_BAD_REQUEST = 1,
	AS_VECTOR_REQ_BAD_CONFIG = 2,
	AS_VECTOR_REQ_UNSUPPORTED_VERSION = 3,
	AS_VECTOR_REQ_LIMIT_EXCEEDED = 4,
	AS_VECTOR_REQ_RESPONSE_TOO_LARGE = 5,
	AS_VECTOR_REQ_SERVER_ERROR = 6
} as_vector_request_status;

typedef enum {
	AS_VECTOR_KEY_OK = 0,
	AS_VECTOR_KEY_WRONG_OWNER = 1,
	AS_VECTOR_KEY_NOT_FOUND = 2,
	AS_VECTOR_KEY_BIN_NOT_FOUND = 3,
	AS_VECTOR_KEY_BAD_BIN_TYPE = 4,
	AS_VECTOR_KEY_MALFORMED_POSTING = 5
} as_vector_key_status;

uint32_t
as_vector_value_type_size(as_vector_value_type type);

bool
as_vector_value_type_from_string(const char* s, as_vector_value_type* out);

bool
as_vector_metric_from_string(const char* s, as_vector_metric* out);

bool
as_vector_namespace_config_valid(const struct as_namespace_s* ns);

#ifdef __cplusplus
}
#endif
