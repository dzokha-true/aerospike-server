/*
 * vector_posting.c
 *
 * EC528: SPTAG posting blob parser.
 */
#include "vector/vector_posting.h"

#include <string.h>

#include "vector/vector_byte_order.h"

bool
as_vector_posting_stride(uint32_t dimension, as_vector_value_type value_type,
		uint32_t* stride_out)
{
	uint32_t esz = as_vector_value_type_size(value_type);

	if (esz == 0 || dimension == 0) {
		return false;
	}

	*stride_out = 5 + dimension * esz;
	return true;
}

bool
as_vector_posting_iter_init(as_vector_posting_iter* it, const uint8_t* blob,
		uint32_t blob_size, uint32_t dimension,
		as_vector_value_type value_type)
{
	uint32_t stride = 0;

	if (! as_vector_posting_stride(dimension, value_type, &stride)) {
		return false;
	}

	if (blob_size % stride != 0) {
		return false;
	}

	it->blob = blob;
	it->blob_size = blob_size;
	it->stride = stride;
	it->offset = 0;
	return true;
}

bool
as_vector_posting_iter_next(as_vector_posting_iter* it,
		as_vector_posting_element* elem_out)
{
	if (it->offset + it->stride > it->blob_size) {
		return false;
	}

	const uint8_t* p = it->blob + it->offset;
	int32_t vid = (int32_t)as_vector_read_le32(p);

	if (vid < 0) {
		return false;
	}

	elem_out->vid = vid;
	elem_out->version = p[4];
	elem_out->payload = p + 5;
	it->offset += it->stride;
	return true;
}
