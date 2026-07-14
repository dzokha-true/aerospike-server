# First full Linux build of the phase-3 branch: latent breakage inventory

The vector work (phase 2-3) had only ever been exercised through the
standalone gtest binary on macOS; `make` for asd itself had never run on this
branch (fork CI's workflow_dispatch path is skip-gated without upstream
secrets). The first `docker build` surfaced, in order:

1. Missing build deps in a fresh Ubuntu 22.04: `cmake` (abseil/s2) and
   `gcc-11-plugin-dev` (the aarch64 TSO gcc plugin).
2. `as/src/base/cfg_tree_handlers.cc`: three stray `}` braces that closed
   functions early and orphaned their bodies at file scope
   (apply_namespace, handle_namespace_write_commit_level_override, one
   more). Mechanical scan: a column-0 `}` followed by a blank line and a
   tab-indented statement is always wrong in this file.
3. Root `Makefile` `targetdirs` never got `$(OBJECT_DIR)/vector`, so every
   vector object failed on its dependency file.
4. `as/src/Makefile` vector C++ pattern rule inherited `-std=gnu99` from
   CFLAGS; gcc treats that as an error for C++ under -Werror (clang on
   macOS tolerated it). Fixed with the same `filter-out` the geospatial
   rule uses.
5. `vector_types.h` used `struct as_namespace_s*` in a prototype without a
   forward declaration ([-Werror] on gcc).
6. A `/*` inside a block comment (`base/*` in prose) tripping
   -Werror=comment.

Lesson: "unit tests green" says nothing about the production TU set; land a
Linux image build (docker/Dockerfile) early in any server-side phase.
