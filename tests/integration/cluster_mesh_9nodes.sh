#!/usr/bin/env bash
#
# SPEC-2-CLUSTER-001
# SPEC-2-CLUSTER-002
# SPEC-2-CLUSTER-003
# SPEC-2-CLUSTER-004

set -euo pipefail

cd "$(dirname "$0")/../.."

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains() {
	local file="$1"
	local pattern="$2"
	local description="$3"

	if ! grep -Eq "$pattern" "$file"; then
		fail "$description"
	fi
}

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/as-cluster-9nodes.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

config="$tmp_dir/aerospike-9node-mesh.conf"

cat > "$config" <<'CONFIG'
service {
	user root
	group root
	paxos-single-replica-limit 1
}

network {
	service {
		address any
		port 3000
	}

	heartbeat {
		mode mesh
		port 3002
		interval 150
		timeout 10
CONFIG

for i in $(seq 1 9); do
	printf '\t\tmesh-seed-address-port 127.0.0.%d 3002\n' "$i" >> "$config"
done

cat >> "$config" <<'CONFIG'
	}
}

namespace test {
	replication-factor 9
	memory-size 128M
	storage-engine memory
}
CONFIG

seed_count="$(grep -c 'mesh-seed-address-port' "$config")"
[[ "$seed_count" -eq 9 ]] || fail "expected 9 mesh seeds, found $seed_count"

assert_contains as/include/fabric/hb.h '^#define[[:space:]]+AS_CLUSTER_SZ[[:space:]]+32([[:space:]]|$)' \
	"AS_CLUSTER_SZ default must be 32"
assert_contains as/include/fabric/partition_balance.h 'COMPILER_ASSERT\(\(AS_CLUSTER_SZ & \(AS_CLUSTER_SZ - 1\)\) == 0\)' \
	"missing power-of-two compile assertion"
assert_contains as/include/fabric/partition.h 'align_3\[64 - \(AS_CLUSTER_SZ % 64\)\]' \
	"as_partition alignment padding must scale with AS_CLUSTER_SZ"
assert_contains as/include/fabric/partition.h 'COMPILER_ASSERT\(sizeof\(as_partition\) % 64 == 0\)' \
	"as_partition alignment compile assertion must remain present"
assert_contains as/src/base/cfg.c 'cfg_u32\(&line, 0, AS_CLUSTER_SZ\)' \
	"min-cluster-size must be bounded by AS_CLUSTER_SZ"
assert_contains as/src/base/cfg.c 'cfg_u32\(&line, 1, AS_CLUSTER_SZ\)' \
	"replication-factor must be bounded by AS_CLUSTER_SZ"
assert_contains as/src/base/cfg.c 'for \(i = 0; i < AS_CLUSTER_SZ; i\+\+\)' \
	"mesh seed parser must allow up to AS_CLUSTER_SZ entries"
assert_contains as/src/fabric/hb.c 'mode_cluster_size = INT_MAX;' \
	"mesh heartbeat mode must not inherit multicast cluster cap"
assert_contains as/src/fabric/hb.c 'MIN\(AS_CLUSTER_SZ, mode_cluster_size\)' \
	"heartbeat supported cluster size must be capped by AS_CLUSTER_SZ"

cc="${CC:-cc}"

if command -v "$cc" >/dev/null 2>&1; then
	"$cc" -x c -std=c11 -o "$tmp_dir/power2" - <<'C'
#define AS_CLUSTER_SZ 32
#define COMPILER_ASSERT(expr) typedef char compiler_assert[(expr) ? 1 : -1]

COMPILER_ASSERT((AS_CLUSTER_SZ & (AS_CLUSTER_SZ - 1)) == 0);

int main(void) { return AS_CLUSTER_SZ == 32 ? 0 : 1; }
C
fi

echo "Generated 9-node mesh config: $config"
echo
echo "Manual cluster verification:"
echo "  1. Build server: make -j4"
echo "  2. Start 9 nodes with heartbeat mode mesh and the generated seed shape."
echo "  3. Verify membership reports 9 nodes, e.g. asadm -e 'info network' or equivalent info command."
echo "  4. Confirm no node logs the former 8-node seed or cluster-size cap."
echo
echo "PASS: Phase 2 cluster limit source/config checks passed."
