#!/usr/bin/env bash
# EC528: render the vector config template from env and exec asd in the
# foreground. AEROSPIKE_VECTOR_SIMD is inherited by the asd process
# environment - an invalid value crashes the first VECTOR_DISTANCE request
# loudly by design (kernel selection has no fallback).
set -euo pipefail

export VECTOR_DIM="${VECTOR_DIM:-64}"
export VECTOR_TYPE="${VECTOR_TYPE:-float}"
export VECTOR_METRIC="${VECTOR_METRIC:-l2}"
export REPLICATION_FACTOR="${REPLICATION_FACTOR:-1}"

if [ -n "${NODE_ID:-}" ]; then
    export NODE_ID_LINE="node-id ${NODE_ID}"
else
    export NODE_ID_LINE=""
fi

# MESH_SEED: space-separated host:port pairs for cluster mode.
MESH_SEED_LINES=""
for seed in ${MESH_SEED:-}; do
    host="${seed%%:*}"
    port="${seed##*:}"
    MESH_SEED_LINES="${MESH_SEED_LINES}mesh-seed-address-port ${host} ${port}
		"
done
export MESH_SEED_LINES

envsubst '${NODE_ID_LINE} ${MESH_SEED_LINES} ${VECTOR_DIM} ${VECTOR_TYPE} ${VECTOR_METRIC} ${REPLICATION_FACTOR}' \
    < /etc/aerospike/aerospike-vector.conf.template \
    > /etc/aerospike/aerospike.conf

echo "--- rendered /etc/aerospike/aerospike.conf ---"
grep -n "vector-\|replication-factor\|mesh-seed\|node-id" /etc/aerospike/aerospike.conf || true
echo "--- AEROSPIKE_VECTOR_SIMD=${AEROSPIKE_VECTOR_SIMD:-auto} ---"

exec /usr/bin/asd --foreground --config-file /etc/aerospike/aerospike.conf
