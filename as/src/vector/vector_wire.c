/*
 * vector_wire.c
 *
 * EC528: VECTOR_DISTANCE request/response codec.
 */
#include "vector/vector_wire.h"

#include <string.h>

#include "vector/vector_byte_order.h"

static bool
head_id_valid(int64_t k)
{
	return k >= 0 && k <= AS_VECTOR_HEAD_ID_MAX;
}

int
as_vector_wire_decode_request(const uint8_t* data, uint32_t size,
		as_vector_wire_request* req_out, char* bin_buf, uint32_t bin_buf_cap,
		char* set_buf, uint32_t set_buf_cap, uint8_t* query_buf,
		uint32_t query_buf_cap, int64_t* head_buf, uint32_t head_buf_cap)
{
	if (size < 16) {
		return -1;
	}

	if (data[0] != AS_VECTOR_WIRE_VERSION) {
		return -1;
	}

	uint32_t topk = as_vector_read_le32(data + 4);
	uint16_t bin_len = as_vector_read_le16(data + 8);
	uint16_t set_len = as_vector_read_le16(data + 10);
	uint16_t query_len = as_vector_read_le16(data + 12);
	uint32_t head_count = as_vector_read_le32(data + 14);

	uint32_t off = 18;

	if (topk == 0 || head_count == 0) {
		return -1;
	}

	if (bin_len >= bin_buf_cap || set_len >= set_buf_cap) {
		return -1;
	}

	if (off + bin_len + set_len + query_len + head_count * 8 > size) {
		return -1;
	}

	if (query_len > query_buf_cap || head_count > head_buf_cap) {
		return -1;
	}

	memcpy(bin_buf, data + off, bin_len);
	bin_buf[bin_len] = '\0';
	off += bin_len;

	memcpy(set_buf, data + off, set_len);
	set_buf[set_len] = '\0';
	off += set_len;

	memcpy(query_buf, data + off, query_len);
	off += query_len;

	for (uint32_t i = 0; i < head_count; i++) {
		head_buf[i] = as_vector_read_le64(data + off);
		off += 8;

		if (! head_id_valid(head_buf[i])) {
			return -1;
		}
	}

	// Dedupe head IDs preserving first occurrence.
	uint32_t out_count = 0;

	for (uint32_t i = 0; i < head_count; i++) {
		bool dup = false;

		for (uint32_t j = 0; j < out_count; j++) {
			if (head_buf[j] == head_buf[i]) {
				dup = true;
				break;
			}
		}
		if (! dup) {
			head_buf[out_count++] = head_buf[i];
		}
	}

	req_out->topk = topk;
	req_out->bin_name = bin_buf;
	req_out->bin_name_len = bin_len;
	req_out->set_name = set_buf;
	req_out->set_name_len = set_len;
	req_out->query_bytes = query_buf;
	req_out->query_bytes_len = query_len;
	req_out->head_id_keys = head_buf;
	req_out->head_id_count = out_count;
	return 0;
}

uint32_t
as_vector_wire_encode_response_size(uint32_t result_count,
		uint32_t key_status_count)
{
	// EC528: per result = int64 head + int32 vid + uint8 version + uint8 pad
	// + float32 distance = 18 bytes; per status = int64 head + uint8 status
	// + uint8 reserved[3] = 12 bytes; header = 12 bytes.
	return 12 + result_count * 18 + key_status_count * 12;
}

int
as_vector_wire_encode_response(uint8_t* buf, uint32_t buf_size,
		uint8_t request_status, const as_vector_scored_tail* results,
		uint32_t result_count, const as_vector_wire_key_status* key_statuses,
		uint32_t key_status_count)
{
	uint32_t need = as_vector_wire_encode_response_size(result_count,
			key_status_count);

	if (buf_size < need) {
		return -1;
	}

	buf[0] = AS_VECTOR_WIRE_VERSION;
	buf[1] = request_status;
	buf[2] = 0;
	buf[3] = 0;
	as_vector_write_le32(buf + 4, result_count);
	as_vector_write_le32(buf + 8, key_status_count);

	uint32_t off = 12;

	for (uint32_t i = 0; i < result_count; i++) {
		as_vector_write_le64(buf + off, results[i].head_id_key);
		off += 8;
		as_vector_write_le32(buf + off, (uint32_t)results[i].vid);
		off += 4;
		buf[off++] = results[i].version;
		buf[off++] = 0;
		as_vector_write_le32_f32(buf + off, results[i].distance);
		off += 4;
	}

	for (uint32_t i = 0; i < key_status_count; i++) {
		as_vector_write_le64(buf + off, key_statuses[i].head_id_key);
		off += 8;
		buf[off++] = key_statuses[i].status;
		buf[off++] = 0;
		buf[off++] = 0;
		buf[off++] = 0;
	}

	return (int)need;
}
