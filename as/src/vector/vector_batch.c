/*
 * vector_batch.c
 *
 * EC528: VECTOR_DISTANCE batch handler.
 */
#include "vector/vector_batch.h"

#include <errno.h>
#include <string.h>

#include "citrusleaf/alloc.h"
#include "citrusleaf/cf_digest.h"

#include "base/cfg.h"
#include "base/datamodel.h"
#include "base/index.h"
#include "base/proto.h"
#include "base/security.h"
#include "base/service.h"
#include "base/stats.h"
#include "base/transaction.h"
#include "fabric/partition.h"
#include "storage/storage.h"
#include "vector/vector_digest.h"
#include "vector/vector_distance.h"
#include "vector/vector_posting.h"
#include "vector/vector_topk.h"
#include "vector/vector_types.h"
#include "vector/vector_wire.h"


static int
send_vector_msg(as_transaction* btr, uint8_t result_code, const uint8_t* payload,
		uint32_t payload_sz)
{
	uint32_t msg_sz = (uint32_t)(sizeof(as_msg) +
			(payload != NULL ? sizeof(as_msg_field) + payload_sz : 0));
	cl_msg* m = cf_malloc(sizeof(as_proto) + msg_sz);

	if (m == NULL) {
		return -1;
	}

	memset(m, 0, sizeof(as_proto) + msg_sz);
	m->proto.version = PROTO_VERSION;
	m->proto.type = PROTO_TYPE_AS_MSG;
	m->proto.sz = msg_sz;
	as_proto_swap(&m->proto);

	m->msg.header_sz = sizeof(as_msg);
	m->msg.info1 = AS_MSG_INFO1_BATCH;
	m->msg.info2 = 0;
	m->msg.info3 = AS_MSG_INFO3_LAST;
	m->msg.info4 = 0;
	m->msg.result_code = result_code;
	m->msg.n_fields = payload != NULL ? 1 : 0;
	m->msg.n_ops = 0;
	as_msg_swap_header(&m->msg);

	if (payload != NULL) {
		as_msg_field* f = (as_msg_field*)m->msg.data;
		f->field_sz = payload_sz + 1;
		f->type = AS_MSG_FIELD_TYPE_VECTOR_DISTANCE_RESPONSE;
		memcpy(f->data, payload, payload_sz);
		as_msg_swap_field(f);
	}

	cf_socket* sock = &btr->from.proto_fd_h->sock;
	int status = 0;

	if (cf_socket_send_all(sock, (uint8_t*)m, sizeof(as_proto) + msg_sz,
				MSG_NOSIGNAL, CF_SOCKET_TIMEOUT) < 0) {
		status = -1;
	}

	as_end_of_transaction(btr->from.proto_fd_h, status != 0);
	btr->from.proto_fd_h = NULL;
	cf_free(btr->msgp);
	btr->msgp = NULL;
	cf_free(m);
	return status;
}

static int
send_vector_error(as_transaction* btr, uint8_t result_code)
{
	return send_vector_msg(btr, result_code, NULL, 0);
}

int
as_vector_batch_handle(as_transaction* btr)
{
	as_msg* msg = &btr->msgp->msg;
	as_msg_field* ns_f = as_msg_field_get(msg, AS_MSG_FIELD_TYPE_NAMESPACE);
	as_msg_field* vf = as_msg_field_get(msg, AS_MSG_FIELD_TYPE_VECTOR_DISTANCE);

	if (ns_f == NULL || vf == NULL) {
		return send_vector_error(btr, AS_ERR_PARAMETER);
	}

	uint32_t ns_len = as_msg_field_get_value_sz(ns_f);
	char ns_name[AS_ID_NAMESPACE_SZ];

	if (ns_len >= sizeof(ns_name)) {
		return send_vector_error(btr, AS_ERR_PARAMETER);
	}

	memcpy(ns_name, ns_f->data, ns_len);
	ns_name[ns_len] = '\0';

	as_namespace* ns = as_namespace_get_byname(ns_name);

	if (ns == NULL) {
		return send_vector_error(btr, AS_ERR_NAMESPACE);
	}

	if (! as_vector_namespace_config_valid(ns)) {
		uint8_t pay[12] = { AS_VECTOR_WIRE_VERSION, AS_VECTOR_REQ_BAD_CONFIG };
		return send_vector_msg(btr, AS_OK, pay, sizeof(pay));
	}

	uint32_t payload_sz = vf->field_sz > 0 ? vf->field_sz - 1 : 0;
	const uint8_t* payload = vf->data;

	// EC528: distinguish unsupported wire version from generic bad-request.
	// The decoder returns -1 for both; check the version byte first so the
	// client sees the contract-defined status and can negotiate.
	if (payload_sz < 1 || payload[0] != AS_VECTOR_WIRE_VERSION) {
		uint8_t pay[12] = { AS_VECTOR_WIRE_VERSION,
				AS_VECTOR_REQ_UNSUPPORTED_VERSION };
		return send_vector_msg(btr, AS_OK, pay, sizeof(pay));
	}

	char bin_buf[256];
	char set_buf[256];
	uint8_t query_buf[1024 * 1024];
	int64_t head_buf[4096];
	as_vector_wire_request req;

	if (as_vector_wire_decode_request(payload, payload_sz, &req, bin_buf,
				sizeof(bin_buf), set_buf, sizeof(set_buf), query_buf,
				sizeof(query_buf), head_buf, sizeof(head_buf) /
						sizeof(head_buf[0])) != 0) {
		uint8_t pay[12] = { AS_VECTOR_WIRE_VERSION, AS_VECTOR_REQ_BAD_REQUEST };
		return send_vector_msg(btr, AS_OK, pay, sizeof(pay));
	}

	uint32_t expect_query = ns->vector_dimension *
			as_vector_value_type_size((as_vector_value_type)ns->vector_value_type);

	if (req.query_bytes_len != expect_query) {
		uint8_t pay[12] = { AS_VECTOR_WIRE_VERSION, AS_VECTOR_REQ_BAD_REQUEST };
		return send_vector_msg(btr, AS_OK, pay, sizeof(pay));
	}

	if (ns->vector_max_query_bytes != 0 &&
			req.query_bytes_len > ns->vector_max_query_bytes) {
		uint8_t pay[12] = { AS_VECTOR_WIRE_VERSION, AS_VECTOR_REQ_LIMIT_EXCEEDED };
		return send_vector_msg(btr, AS_OK, pay, sizeof(pay));
	}

	if (ns->vector_max_topk != 0 && req.topk > ns->vector_max_topk) {
		uint8_t pay[12] = { AS_VECTOR_WIRE_VERSION, AS_VECTOR_REQ_LIMIT_EXCEEDED };
		return send_vector_msg(btr, AS_OK, pay, sizeof(pay));
	}

	if (ns->vector_max_head_ids != 0 &&
			req.head_id_count > ns->vector_max_head_ids) {
		uint8_t pay[12] = { AS_VECTOR_WIRE_VERSION, AS_VECTOR_REQ_LIMIT_EXCEEDED };
		return send_vector_msg(btr, AS_OK, pay, sizeof(pay));
	}

	uint32_t resp_sz = as_vector_wire_encode_response_size(req.topk,
			req.head_id_count);
	if (ns->vector_max_response_bytes != 0 &&
			resp_sz > ns->vector_max_response_bytes) {
		uint8_t pay[12] = { AS_VECTOR_WIRE_VERSION,
				AS_VECTOR_REQ_RESPONSE_TOO_LARGE };
		return send_vector_msg(btr, AS_OK, pay, sizeof(pay));
	}

	as_vector_scored_tail* scored =
			cf_malloc(req.topk * sizeof(as_vector_scored_tail));
	as_vector_wire_key_status* statuses =
			cf_malloc(req.head_id_count * sizeof(as_vector_wire_key_status));
	uint8_t* resp_buf = cf_malloc(resp_sz);

	if (scored == NULL || statuses == NULL || resp_buf == NULL) {
		cf_free(scored);
		cf_free(statuses);
		cf_free(resp_buf);
		return send_vector_error(btr, AS_ERR_UNKNOWN);
	}

	as_vector_topk acc;
	as_vector_topk_init(&acc, scored, req.topk, req.topk);

	uint32_t status_count = 0;
	// EC528: as_storage_rd_load_bins() unpacks up to rd->flat_n_bins (capped
	// only by RECORD_MAX_BINS) into the caller buffer; match the upstream
	// read path so a many-bin record cannot overflow the stack.
	as_bin stack_bins[RECORD_MAX_BINS];

	for (uint32_t hi = 0; hi < req.head_id_count; hi++) {
		int64_t head_id = req.head_id_keys[hi];
		cf_digest keyd;

		as_vector_digest_compute(ns_name, set_buf, head_id, &keyd);
		uint32_t pid = as_partition_getid(&keyd);
		as_partition_reservation rsv;

		as_partition_reserve(ns, pid, &rsv);

		if (as_partition_writable_node(ns, pid) != g_config.self_node) {
			statuses[status_count].head_id_key = head_id;
			statuses[status_count].status = AS_VECTOR_KEY_WRONG_OWNER;
			status_count++;
			as_partition_release(&rsv);
			continue;
		}

		as_index_ref r_ref;

		if (as_record_get(rsv.tree, &keyd, &r_ref) != 0) {
			statuses[status_count].head_id_key = head_id;
			statuses[status_count].status = AS_VECTOR_KEY_NOT_FOUND;
			status_count++;
			as_partition_release(&rsv);
			continue;
		}

		as_storage_rd rd;

		as_storage_record_open(ns, r_ref.r, &rd);

		if (as_storage_rd_load_bins(&rd, stack_bins) < 0) {
			as_storage_record_close(&rd);
			as_record_done(&r_ref, ns);
			as_partition_release(&rsv);
			statuses[status_count].head_id_key = head_id;
			statuses[status_count].status = AS_VECTOR_KEY_NOT_FOUND;
			status_count++;
			continue;
		}

		as_bin* b = as_bin_get_live(&rd, req.bin_name);

		if (b == NULL) {
			as_storage_record_close(&rd);
			as_record_done(&r_ref, ns);
			as_partition_release(&rsv);
			statuses[status_count].head_id_key = head_id;
			statuses[status_count].status = AS_VECTOR_KEY_BIN_NOT_FOUND;
			status_count++;
			continue;
		}

		as_particle_type ptype = as_bin_get_particle_type(b);

		if (ptype != AS_PARTICLE_TYPE_BLOB && ptype != AS_PARTICLE_TYPE_VECTOR) {
			as_storage_record_close(&rd);
			as_record_done(&r_ref, ns);
			as_partition_release(&rsv);
			statuses[status_count].head_id_key = head_id;
			statuses[status_count].status = AS_VECTOR_KEY_BAD_BIN_TYPE;
			status_count++;
			continue;
		}

		uint8_t* blob = NULL;
		uint32_t blob_sz = as_bin_particle_blob_ptr(b, &blob);

		as_vector_posting_iter pit;
		bool malformed = false;

		if (! as_vector_posting_iter_init(&pit, blob, blob_sz, ns->vector_dimension,
					(as_vector_value_type)ns->vector_value_type)) {
			malformed = true;
		}
		else {
			as_vector_posting_element elem;

			while (as_vector_posting_iter_next(&pit, &elem)) {
				float dist = 0.0f;

				if (as_vector_distance_compute(
							(as_vector_value_type)ns->vector_value_type,
							(as_vector_metric)ns->vector_metric,
							ns->vector_dimension,
							req.query_bytes, elem.payload, &dist) != 0) {
					malformed = true;
					break;
				}

				as_vector_scored_tail st = {
					.head_id_key = head_id,
					.vid = elem.vid,
					.version = elem.version,
					.distance = dist
				};

				as_vector_topk_add(&acc, &st);
			}
		}

		// EC528: iter sets malformed when next() rejects negative VID, so the
		// loop ending alone does not imply success.
		if (pit.malformed) {
			malformed = true;
		}

		as_storage_record_close(&rd);
		as_record_done(&r_ref, ns);
		as_partition_release(&rsv);

		if (malformed) {
			statuses[status_count].head_id_key = head_id;
			statuses[status_count].status = AS_VECTOR_KEY_MALFORMED_POSTING;
			status_count++;
		}
	}

	uint32_t result_count = as_vector_topk_fill(&acc, scored, req.topk);

	int encoded = as_vector_wire_encode_response(resp_buf, resp_sz,
			AS_VECTOR_REQ_OK, scored, result_count, statuses, status_count);

	if (encoded < 0) {
		cf_free(scored);
		cf_free(statuses);
		cf_free(resp_buf);
		return send_vector_error(btr, AS_ERR_UNKNOWN);
	}

	// EC528: send only the bytes the encoder wrote; resp_sz is the max-cap
	// allocation, so sending it would leak uninitialized heap.
	int rv = send_vector_msg(btr, AS_OK, resp_buf, (uint32_t)encoded);

	cf_free(scored);
	cf_free(statuses);
	cf_free(resp_buf);
	return rv;
}
