# Rulings: 002-server-a1-kernel-dispatch (append-only, orchestrator-owned)

- 2026-07-14 RULING (mode change): repo owner directed in-session that the
  orchestrator implements all slices directly ("continue, I want you to
  implement the changes (not codex)"). Builder dispatch and cold judges are
  waived by the human for this run; frozen checks remain the acceptance
  gate, run by the orchestrator, evidence recorded per slice.
- 2026-07-14 RULING (design): `as_vector_distance_compute()` stays pinned to
  the SCALAR kernel row instead of following AEROSPIKE_VECTOR_SIMD. Why: it
  is the oracle the existing math tests and future SIMD parity tests compare
  against; letting it float with the active ISA would make the oracle
  self-referential and break bit-stable tests when NEON registers (#3). The
  production hot path resolves the active kernel separately in
  vector_batch.c.
