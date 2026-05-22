/*
 * vector_posting_test.cc
 *
 * SPEC-3-PARSER-001: posting blob parser.
 */
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "vector/vector_byte_order.h"
#include "vector/vector_posting.h"

namespace {

void
append_le32(std::vector<uint8_t>& buf, int32_t v)
{
	uint8_t tmp[4];
	as_vector_write_le32(tmp, (uint32_t)v);
	buf.insert(buf.end(), tmp, tmp + 4);
}

} // namespace

TEST(VectorPosting, ParseTwoElements) // SPEC-3-PARSER-001
{
	const uint32_t dim = 2;
	std::vector<uint8_t> blob;

	append_le32(blob, 7);
	blob.push_back(1);
	const float p0[] = { 0.0f, 0.0f };
	const uint8_t* p0b = reinterpret_cast<const uint8_t*>(p0);
	blob.insert(blob.end(), p0b, p0b + sizeof(p0));

	append_le32(blob, 9);
	blob.push_back(2);
	const float p1[] = { 1.0f, 0.0f };
	const uint8_t* p1b = reinterpret_cast<const uint8_t*>(p1);
	blob.insert(blob.end(), p1b, p1b + sizeof(p1));

	as_vector_posting_iter it;
	ASSERT_TRUE(as_vector_posting_iter_init(&it, blob.data(),
			(uint32_t)blob.size(), dim, AS_VECTOR_VALUE_TYPE_FLOAT));

	as_vector_posting_element e;

	ASSERT_TRUE(as_vector_posting_iter_next(&it, &e));
	EXPECT_EQ(7, e.vid);
	EXPECT_EQ(1, e.version);
	EXPECT_EQ(0, e.payload[0]);

	ASSERT_TRUE(as_vector_posting_iter_next(&it, &e));
	EXPECT_EQ(9, e.vid);
	EXPECT_EQ(2, e.version);
	EXPECT_FALSE(as_vector_posting_iter_next(&it, &e));
}

TEST(VectorPosting, MalformedLength) // SPEC-3-PARSER-001
{
	uint8_t blob[7] = { 0 };

	as_vector_posting_iter it;
	EXPECT_FALSE(as_vector_posting_iter_init(&it, blob, sizeof(blob), 2,
			AS_VECTOR_VALUE_TYPE_FLOAT));
}

TEST(VectorPosting, NegativeVid) // SPEC-3-PARSER-001
{
	std::vector<uint8_t> blob;

	append_le32(blob, -1);
	blob.push_back(0);
	const float p[] = { 0.0f, 0.0f };
	const uint8_t* pb = reinterpret_cast<const uint8_t*>(p);
	blob.insert(blob.end(), pb, pb + sizeof(p));

	as_vector_posting_iter it;
	ASSERT_TRUE(as_vector_posting_iter_init(&it, blob.data(),
			(uint32_t)blob.size(), 2, AS_VECTOR_VALUE_TYPE_FLOAT));

	as_vector_posting_element e;
	EXPECT_FALSE(as_vector_posting_iter_next(&it, &e));
}
