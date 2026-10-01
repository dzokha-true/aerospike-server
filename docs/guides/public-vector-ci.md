# Public vector verification

`.github/workflows/public-vector-ci.yml` builds the triggering community
checkout on native x86 Linux using public submodules and packages. It runs
vector gtests, Python wire/digest tests and the documentation contract check,
then uploads compiler logs, runner CPU details and gtest XML even on failure.
It does not depend on an Enterprise checkout or application credentials.
The existing upstream EE workflow is unchanged.

SSE parity must execute. AVX2 and AVX512 remain gated by actual CPU support;
a skipped suite is not a verified ISA. The XML and detection log show which
paths ran. This workflow is a focused vector/CE build check, not all server
integration coverage, a multi-node benchmark or a production qualification.

## Reproduce on Ubuntu 24.04

Use full history. The fork has no release tags; `build/version` otherwise
falls back to a bare SHA, which the current public C client rejects as an
invalid server version even when the server accepts TCP connections.

```bash
git submodule update --init --recursive
git fetch --no-recurse-submodules https://github.com/aerospike/aerospike-server.git refs/tags/8.1.1.2:refs/tags/8.1.1.2
test "$(git rev-parse '8.1.1.2^{commit}')" = 783cd07537a83bae5aaa851cc1a6f7244e8e3b73
git merge-base --is-ancestor 8.1.1.2 HEAD
bash .github/bin/install_deps.bash ubuntu24.04
make -j2
GTEST_OUTPUT=xml:vector-tests.xml make -C as run-vector-tests
OPENSSL_CONF=tests/conformance/vector_distance/openssl-test.cnf \
  python3 -m unittest -v tests.conformance.vector_distance.test_codec
bash tests/docs/check_docs.sh
```

The OpenSSL configuration applies only to that test process. Aerospike's
integer-key digest requires RIPEMD-160; Ubuntu/OpenSSL 3 may expose it only
through the legacy provider. It is not a change to server TLS configuration.

For the end-to-end public client build and seeded search smoke, see the
SPTAG research branch's `.github/workflows/public-offload-ci.yml` and
`docs/benchmarks/reanalysis-2026-10-01.md`.
