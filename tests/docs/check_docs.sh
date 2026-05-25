#!/usr/bin/env bash
# EC528: fail if required documentation harness files are missing.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

required=(
  CONTEXT.md
  AGENTS.md
  docs/contracts/sptag-aerospike.md
  docs/architecture/module-map.md
  docs/specs/phase-2-cluster.md
  docs/specs/phase-3-vector-distance.md
  docs/specs/phase-4-integration.md
  docs/adr/0001-hybrid-sptag-storage-split.md
  docs/adr/0002-vector-bin-format.md
  docs/adr/0003-cluster-size-limit.md
  docs/adr/0004-vector-distance-protocol.md
  docs/agent-phases/00-spec-and-test-driven.md
)

missing=0
for f in "${required[@]}"; do
  if [[ ! -f "$f" ]]; then
    echo "missing: $f" >&2
    missing=1
  fi
done

if [[ "$missing" -ne 0 ]]; then
  exit 1
fi

echo "check_docs: ok (${#required[@]} files)"
