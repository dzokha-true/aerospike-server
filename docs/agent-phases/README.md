# Agent phase handoff documents

Give **one file per task** to a coding agent. Each file is self-contained (zero prior context assumed).

**Mandatory workflow:** [00-spec-and-test-driven.md](./00-spec-and-test-driven.md) — spec before code, failing test before impl.

## Order

| File | When to run |
|------|-------------|
| [phase-1-documentation-harness.md](./phase-1-documentation-harness.md) | First — creates docs other phases depend on |
| [phase-2-cluster-limit.md](./phase-2-cluster-limit.md) | Parallel OK after Phase 1 contract draft exists |
| [phase-3-vector-distance.md](./phase-3-vector-distance.md) | After Phase 1 contract defines posting layout (shipped on `ec528/phase-3-vector-distance`) |
| [phase-4-sptag-integration.md](./phase-4-sptag-integration.md) | After Phase 3 ops exist; needs SPTAG repo access |

| [00-spec-and-test-driven.md](./00-spec-and-test-driven.md) | **Read before every phase** |
| [00-project-context.md](./00-project-context.md) | Optional architecture primer |

## Repo

Fork of **Aerospike Database Server CE** (AGPL). Tag local changes with `// EC528:`.

## Master plan

See `.cursor/plans/sptag_aerospike_restart_*.plan.md` if present in your environment.
