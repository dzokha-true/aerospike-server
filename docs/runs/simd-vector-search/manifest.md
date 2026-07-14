---
run: simd-vector-search
tracking-issue: 1
factory-branch: factory/simd-vector-search
tracker: markdown
spec: docs/spec/aerospike-simd-vector-search.md
state: FINISHED
created: 2026-07-14
---

Coordination home for the simd-vector-search run (multi-repo). This repo holds
the tracker (docs/issues/simd-vector-search/), the spec copy, and checks/jobs
for server-lane slices. SPTAG-lane slices carry their checks and job reports in
SPTAG-upstream (same branch name `factory/simd-vector-search`, base `1b4f9b2`);
their issue files still live here. aerospike-client-c is not expected to
change; any client defect found is a new issue, not an in-place edit.

Server-lane freeze base: ed703a2 on factory/simd-vector-search.
SPTAG-lane freeze base: 1b4f9b2 on factory/simd-vector-search (SPTAG-upstream).
