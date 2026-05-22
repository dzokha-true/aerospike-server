/*
 * vector_wire_test.cc
 *
 * SPEC-3-WIRE-001: request/response codec round-trip.
 */
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "vector/vector_topk.h"
#include "vector/vector_wire.h"

TEST(VectorWire, RequestRoundTrip) // SPEC-3-WIRE-001
{
	uint8_t req_buf[256];
	uint32_t off = 0;

	req_buf[off++] = AS_VECTOR_WIRE_VERSION;
	req_buf[off++] = 0;
	req_buf[off++] = 0;
	req_buf[off++] = 0;
	memcpy(req_buf + off, "\x02\x00\x00\x00", 4);
	off += 4;
	memcpy(req_buf + off, "\x03\x00", 2);
	off += 2;
	memcpy(req_buf + off, "\x00\x00", 2);
	off += 2;
	memcpy(req_buf + off, "\x08\x00", 2);
	off += 2;
	memcpy(req_buf + off, "\x01\x00\x00\x00", 4);
	off += 4;
	memcpy(req_buf + off, "vec", 3);
	off += 3;
	const float qv[] = { 1.0f, 2.0f };
	memcpy(req_buf + off, qv, sizeof(qv));
	off += sizeof(qv);
	memcpy(req_buf + off, "\x2a\x00\x00\x00\x00\x00\x00\x00", 8);
	off += 8;

	char bin[32];
	char set[32];
	uint8_t query[32];
	int64_t heads[4];
	as_vector_wire_request req;

	ASSERT_EQ(0, as_vector_wire_decode_request(req_buf, off, &req, bin,
			sizeof(bin), set, sizeof(set), query, sizeof(query), heads,
			sizeof(heads) / sizeof(heads[0])));
	EXPECT_EQ(2u, req.topk);
	EXPECT_STREQ("vec", req.bin_name);
	EXPECT_EQ(8u, req.query_bytes_len);
	EXPECT_EQ(1u, req.head_id_count);
	EXPECT_EQ(42, req.head_id_keys[0]);
}

TEST(VectorWire, RejectEmptyHeadList) // SPEC-3-WIRE-001
{
	uint8_t req_buf[32] = { AS_VECTOR_WIRE_VERSION, 0, 0, 0, 1, 0, 0, 0,
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	char bin[8];
	char set[8];
	uint8_t query[8];
	int64_t heads[1];
	as_vector_wire_request req;

	EXPECT_NE(0, as_vector_wire_decode_request(req_buf, sizeof(req_buf), &req,
			bin, sizeof(bin), set, sizeof(set), query, sizeof(query), heads, 1));
}

TEST(VectorWire, ResponseEncode) // SPEC-3-WIRE-001
{
	as_vector_scored_tail results[] = { { 1, 7, 2, 0.25f } };
	as_vector_wire_key_status statuses[] = { { 2, AS_VECTOR_KEY_WRONG_OWNER } };
	uint8_t buf[64];

	ASSERT_EQ(40, as_vector_wire_encode_response(buf, sizeof(buf),
			AS_VECTOR_REQ_OK, results, 1, statuses, 1));
	EXPECT_EQ(AS_VECTOR_WIRE_VERSION, buf[0]);
	EXPECT_EQ(AS_VECTOR_REQ_OK, buf[1]);
}
