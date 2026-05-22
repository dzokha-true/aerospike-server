/*
 * vector_namespace_server.c
 *
 * EC528: bridge vector config validation to as_namespace.
 */
#include "vector/vector_namespace_cfg.h"
#include "vector/vector_types.h"

#include "base/datamodel.h"

bool
as_vector_namespace_config_valid(const as_namespace* ns)
{
	as_vector_namespace_cfg cfg = {
		.vector_dimension = ns->vector_dimension,
		.vector_value_type = ns->vector_value_type,
		.vector_metric = ns->vector_metric,
		.vector_max_query_bytes = ns->vector_max_query_bytes
	};
	return as_vector_namespace_cfg_valid(&cfg);
}
