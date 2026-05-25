# Spec-driven + test-driven workflow (all phases)

Every coding agent **must** follow this process. Specs are the source of truth; tests prove the spec.

---

## Two artifact types

| Type | Location | Purpose |
|------|----------|---------|
| **Contract spec** | `docs/contracts/` | Wire format, byte layout, opcodes, keys — machine-oriented |
| **Behavior spec** | `docs/specs/` | Given/When/Then scenarios, invariants, error cases — human + agent oriented |

**Rule:** No production C++ for a feature until a behavior spec section exists and at least one **failing** test is written that cites the spec ID.

---

## Spec ID format

```
SPEC-<phase>-<topic>-<nnn>
```

Examples:

- `SPEC-2-CLUSTER-001` — cluster accepts 9 nodes in mesh mode
- `SPEC-3-VDIST-002` — L2 distance on synthetic posting matches reference
- `SPEC-4-INTEG-001` — top-K overlap ≥99% vs client ComputeDistance baseline

Reference spec IDs in:

- Test file comments: `// SPEC-3-VDIST-002`
- Commit messages
- PR descriptions

---

## Agent loop (red → green → refactor)

```mermaid
flowchart LR
  ReadSpec[Read spec + contract]
  Red[Write failing test]
  Green[Minimal impl]
  Refactor[Refactor]
  UpdateSpec[Mark spec verified]
  ReadSpec --> Red --> Green --> Refactor --> UpdateSpec
```

1. **Read** relevant `docs/specs/*.md` + `docs/contracts/*.md`
2. **Red** — add test that fails for the right reason (compile fail or assertion fail)
3. **Green** — smallest code change to pass
4. **Refactor** — only with tests green
5. **Update** spec frontmatter: `status: verified` + link to test file

**Forbidden:** implement first, spec later; delete failing tests to green CI.

---

## Test layers

| Layer | When | Where (this repo) |
|-------|------|-------------------|
| **Unit** | Pure functions (math, posting parser) | `as/src/vector/*_test.cc` (gtest — add target if missing) |
| **Conformance** | Spec scenarios against running server | `tests/conformance/` or `as/src/vector/conformance/` |
| **Integration** | Multi-node cluster (Phase 2, 4) | `docs/specs/` defines steps; script in `tests/integration/` |
| **Doc lint** | Phase 1 links, required sections | `tests/docs/check_docs.sh` (agent may create) |

Upstream CE has **few** server unit tests. You will **add** gtest targets (deps: `libgtest-dev`, see `bin/install-dependencies.sh`).

---

## Spec template (`docs/specs/_template.md`)

```markdown
---
id: SPEC-3-EXAMPLE-001
phase: 3
status: draft   # draft | implemented | verified
contract: docs/contracts/sptag-aerospike.md#vector-distance
tests: []
---

# Title

## Given
...

## When
...

## Then
...

## Error cases
- ...
```

---

## Contract ↔ test alignment

| Contract section | Minimum tests |
|------------------|---------------|
| Posting byte layout | Parser unit tests with hex fixtures from SPTAG |
| `VECTOR_DISTANCE` wire format | `vector_wire_test.cc` + `tests/conformance/vector_distance/test_codec.py`; live asd smoke planned Phase 4 |
| `AS_CLUSTER_SZ` | Integration spec: N nodes join mesh cluster |

When contract changes, **update spec + tests in same PR**.

---

## CI expectation

- Phase 1: `check_docs.sh` passes (required files, internal links)
- Phase 2+: build + unit tests green
- Phase 4: conformance + benchmark results committed to `docs/benchmarks/`

See `.github/workflows/build-and-test.yaml` — extend if needed with `// EC528:` jobs.

---

## Phase summary

| Phase | Spec-first deliverable | Test-first deliverable |
|-------|------------------------|-------------------------|
| 1 | All specs + contracts drafted | `check_docs.sh`, spec ID registry |
| 2 | `SPEC-2-CLUSTER-*` | Integration script or documented manual test |
| 3 | `SPEC-3-VDIST-*`, `SPEC-3-PARSER-*` | gtest unit + conformance |
| 4 | `SPEC-4-INTEG-*` | A/B correctness + benchmark spec |

---

## Anti-patterns

- “Tests optional for doc-only phase” — Phase 1 uses doc lint + spec completeness checks
- Benchmarks without correctness spec (Phase 4)
- Specs that restate code line-by-line instead of behavior
- UDF-based tests for hot path (use native op conformance)
