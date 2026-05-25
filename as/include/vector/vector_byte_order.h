/*
 * vector_byte_order.h
 *
 * EC528: little-endian helpers for vector wire/posting (Phase 3 LE only).
 */
#pragma once

#include <stdint.h>
#include <string.h>

static inline uint16_t
as_vector_read_le16(const uint8_t* p)
{
	uint16_t v;
	memcpy(&v, p, sizeof(v));
	return v;
}

static inline uint32_t
as_vector_read_le32(const uint8_t* p)
{
	uint32_t v;
	memcpy(&v, p, sizeof(v));
	return v;
}

static inline int64_t
as_vector_read_le64(const uint8_t* p)
{
	int64_t v;
	memcpy(&v, p, sizeof(v));
	return v;
}

static inline void
as_vector_write_le16(uint8_t* p, uint16_t v)
{
	memcpy(p, &v, sizeof(v));
}

static inline void
as_vector_write_le32(uint8_t* p, uint32_t v)
{
	memcpy(p, &v, sizeof(v));
}

static inline void
as_vector_write_le64(uint8_t* p, int64_t v)
{
	memcpy(p, &v, sizeof(v));
}

static inline void
as_vector_write_le32_f32(uint8_t* p, float f)
{
	uint32_t u;
	memcpy(&u, &f, sizeof(u));
	as_vector_write_le32(p, u);
}
