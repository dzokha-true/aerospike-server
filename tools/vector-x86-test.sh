#!/usr/bin/env bash
# EC528: run the vector gtest suite inside an x86_64 Linux container on this
# arm64 host (Rosetta/qemu emulation). SSE parity must run; AVX2 runs when
# the emulator exposes it (Rosetta on macOS 15 does); AVX-512 is expected to
# skip. Prints X86_TESTS_OK on success. Used by the frozen checks for
# issues #4/#5.
set -euo pipefail

cd "$(dirname "$0")/.."

docker run --rm --platform linux/amd64 \
    -v "$PWD":/w -w /w \
    ubuntu:22.04 bash -c '
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null
apt-get install -y -qq build-essential libgtest-dev >/dev/null
make -C as run-vector-tests
'

echo "X86_TESTS_OK"
