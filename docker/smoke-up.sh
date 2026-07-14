#!/usr/bin/env bash
# EC528: bring up the single-node vector server, verify readiness and the
# live vector namespace config, tear down. Prints SERVER_VECTOR_READY on
# success. Used by the frozen check for issue #8.
set -euo pipefail

cd "$(dirname "$0")"

COMPOSE="docker compose -f compose-single.yml"

cleanup() {
    $COMPOSE down -v >/dev/null 2>&1 || true
}
trap cleanup EXIT

$COMPOSE up -d

echo "waiting for asd readiness..."
ready=0
for _ in $(seq 1 60); do
    if docker logs as-vector-single 2>&1 | grep -q "service ready: soon there will be cake"; then
        ready=1
        break
    fi
    if [ -z "$(docker ps -q -f name=as-vector-single)" ]; then
        echo "FATAL: asd container exited early; last log lines:" >&2
        docker logs as-vector-single 2>&1 | tail -25 >&2
        exit 1
    fi
    sleep 2
done

if [ "$ready" -ne 1 ]; then
    echo "FATAL: asd not ready after 120s; last log lines:" >&2
    docker logs as-vector-single 2>&1 | tail -25 >&2
    exit 1
fi

# The rendered config must carry the vector namespace stanza.
docker exec as-vector-single grep -q "vector-dimension" /etc/aerospike/aerospike.conf
docker exec as-vector-single grep -q "vector-value-type" /etc/aerospike/aerospike.conf
docker exec as-vector-single grep -q "vector-metric" /etc/aerospike/aerospike.conf

echo "SERVER_VECTOR_READY"
