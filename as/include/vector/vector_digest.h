/*
 * vector_digest.h
 *
 * EC528: Compute Aerospike record digest for SPTAG head_id_key.
 */
#pragma once

#include "citrusleaf/cf_digest.h"

#ifdef __cplusplus
extern "C" {
#endif

void
as_vector_digest_compute(const char* ns_name, const char* set_name,
		int64_t head_id_key, cf_digest* keyd);

#ifdef __cplusplus
}
#endif
