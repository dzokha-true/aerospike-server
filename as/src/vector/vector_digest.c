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

// EC528: Compute the canonical Aerospike record digest for an int64 head_id.
// Standard layout (matches the client's as_key_init_int64 / as_key_set_digest):
//
//   RIPEMD-160( set_name_bytes || type_byte (AS_PARTICLE_TYPE_INTEGER=1)
//                              || big-endian int64 key )
//
// The namespace is NOT part of the digest; it identifies the partition tree.
// Including it here would make every server-computed digest miss the records
// SPTAG wrote with the standard client API.
void
as_vector_digest_compute(const char* ns_name, const char* set_name,
		int64_t head_id_key, cf_digest* keyd)
{
	(void)ns_name;
	const char* set = set_name != NULL ? set_name : "";
	uint32_t set_len = (uint32_t)strlen(set);
	uint8_t buf[AS_SET_NAME_MAX_SIZE + 1 + sizeof(uint64_t)];
	uint32_t off = 0;

	if (set_len > AS_SET_NAME_MAX_SIZE) {
		set_len = AS_SET_NAME_MAX_SIZE;
	}
	memcpy(buf + off, set, set_len);
	off += set_len;

	buf[off++] = AS_PARTICLE_TYPE_INTEGER;
	uint64_t be_key = cf_swap_to_be64((uint64_t)head_id_key);
	memcpy(buf + off, &be_key, sizeof(be_key));
	off += sizeof(be_key);

	cf_digest_compute(buf, off, keyd);
}
