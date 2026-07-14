#!/usr/bin/env bash
# EC528: live smoke - runs the wire-level VECTOR_DISTANCE cases against a
# scalar-kernel container and a neon-kernel container, then compares the two
# snapshots (statuses identical, result sets identical, distances within
# 1e-5 relative). Prints SMOKE_OK on success. Used by the frozen check for
# issue #10.
set -euo pipefail

cd "$(dirname "$0")/../../.."   # repo root

IMAGE="${SERVER_IMAGE:-as-vector:check}"
OUT_DIR=".architect/tmp/smoke"
mkdir -p "$OUT_DIR"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "building $IMAGE..."
    docker build -f docker/Dockerfile -t "$IMAGE" . >/dev/null
fi

run_mode() {
    local mode="$1" port="$2" out="$3"
    local name="as-vector-smoke-$mode"

    docker rm -f "$name" >/dev/null 2>&1 || true
    docker run -d --name "$name" -p "$port:3000" \
        -e AEROSPIKE_VECTOR_SIMD="$mode" "$IMAGE" >/dev/null

    local ready=0
    for _ in $(seq 1 60); do
        if docker logs "$name" 2>&1 | grep -q "service ready: soon there will be cake"; then
            ready=1
            break
        fi
        if [ -z "$(docker ps -q -f name=$name)" ]; then
            echo "FATAL: $name exited early:" >&2
            docker logs "$name" 2>&1 | tail -20 >&2
            docker rm -f "$name" >/dev/null 2>&1 || true
            return 1
        fi
        sleep 2
    done
    if [ "$ready" -ne 1 ]; then
        echo "FATAL: $name not ready" >&2
        docker rm -f "$name" >/dev/null 2>&1 || true
        return 1
    fi

    python3 -m tests.conformance.vector_distance.smoke run \
        --host 127.0.0.1 --port "$port" --out "$out"
    local rc=$?

    # The chosen ISA must be logged once by the first VECTOR_DISTANCE request.
    docker logs "$name" 2>&1 | grep "VECTOR_DISTANCE kernel isa" | tail -1

    docker rm -f "$name" >/dev/null 2>&1 || true
    return $rc
}

run_mode scalar 3050 "$OUT_DIR/scalar.json"
run_mode neon 3051 "$OUT_DIR/neon.json"

python3 -m tests.conformance.vector_distance.smoke compare \
    "$OUT_DIR/scalar.json" "$OUT_DIR/neon.json"

echo "SMOKE_OK"
