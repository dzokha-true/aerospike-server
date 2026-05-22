/*
 * vector_wire.h
 *
 * EC528: VECTOR_DISTANCE request/response codec.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "vector/vector_topk.h"
#include "vector/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct as_vector_wire_request_s {
	uint32_t topk;
	const char* bin_name;
	uint32_t bin_name_len;
	const char* set_name;
	uint32_t set_name_len;
	const uint8_t* query_bytes;
	uint32_t query_bytes_len;
	const int64_t* head_id_keys;
	uint32_t head_id_count;
} as_vector_wire_request;

typedef struct as_vector_wire_key_status_s {
	int64_t head_id_key;
	uint8_t status;
} as_vector_wire_key_status;

typedef struct as_vector_wire_response_s {
	uint8_t request_status;
	as_vector_scored_tail* results;
	uint32_t result_count;
	as_vector_wire_key_status* key_statuses;
	uint32_t key_status_count;
} as_vector_wire_response;

int
as_vector_wire_decode_request(const uint8_t* data, uint32_t size,
		as_vector_wire_request* req_out, char* bin_buf, uint32_t bin_buf_cap,
		char* set_buf, uint32_t set_buf_cap, uint8_t* query_buf,
		uint32_t query_buf_cap, int64_t* head_buf, uint32_t head_buf_cap);

uint32_t
as_vector_wire_encode_response_size(uint32_t result_count,
		uint32_t key_status_count);

int
as_vector_wire_encode_response(uint8_t* buf, uint32_t buf_size,
		uint8_t request_status, const as_vector_scored_tail* results,
		uint32_t result_count, const as_vector_wire_key_status* key_statuses,
		uint32_t key_status_count);

#ifdef __cplusplus
}
#endif
