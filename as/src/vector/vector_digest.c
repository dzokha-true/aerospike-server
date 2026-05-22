/*
 * vector_digest.c
 *
 * EC528: Compute Aerospike record digest for SPTAG head_id_key.
 */
#include "vector/vector_digest.h"

#include <string.h>

#include "citrusleaf/cf_byte_order.h"
#include "citrusleaf/cf_digest.h"

#include "base/datamodel.h"

void
as_vector_digest_compute(const char* ns_name, const char* set_name,
		int64_t head_id_key, cf_digest* keyd)
{
	const char* set = set_name != NULL ? set_name : "";
	uint32_t ns_len = (uint32_t)strlen(ns_name);
	uint32_t set_len = (uint32_t)strlen(set);
	uint8_t key_buf[1 + sizeof(uint64_t)];
	uint8_t buf[256];
	uint32_t off = 0;

	key_buf[0] = AS_PARTICLE_TYPE_INTEGER;
	*(uint64_t*)(key_buf + 1) = cf_swap_to_be64((uint64_t)head_id_key);

	memcpy(buf + off, ns_name, ns_len);
	off += ns_len;
	memcpy(buf + off, set, set_len);
	off += set_len;
	memcpy(buf + off, key_buf, sizeof(key_buf));
	off += sizeof(key_buf);

	cf_digest_compute(buf, off, keyd);
}
