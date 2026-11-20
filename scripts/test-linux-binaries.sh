#!/usr/bin/env bash
# test-linux-binaries.sh - runs each compiler's test suite against the static
# Linux binaries in server/bin/linux/ (the ones deployed to Vercel).
#
#   bash scripts/test-linux-binaries.sh      (run on Linux or in WSL)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/server/bin/linux"
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK:?}"' EXIT

cp -r "$ROOT/backend/c-compiler" "$ROOT/backend/c++_compiler" "$ROOT/backend/javacompiler" "$WORK/"
rm -rf "${WORK:?}"/*/build
mkdir -p "$WORK/c-compiler/build" "$WORK/c++_compiler/build" "$WORK/javacompiler/build"
cp "$BIN/tricora"   "$WORK/c-compiler/build/tricora"
cp "$BIN/compiler"  "$WORK/c++_compiler/build/compiler.exe"   # the C++ test runner expects this name
cp "$BIN/minijavac" "$WORK/javacompiler/build/minijavac"
# Test scripts checked out on Windows may have CRLF line endings.
find "$WORK" -name '*.sh' -exec sed -i 's/\r$//' {} +

status=0
echo "C:    $(cd "$WORK/c-compiler"   && sh tests/run_tests.sh 2>&1 | tail -1)"   || status=1
echo "C++:  $(cd "$WORK/c++_compiler" && bash tests/run_tests.sh 2>&1 | tail -1)" || status=1
echo "Java: $(cd "$WORK/javacompiler" && sh tests/run_tests.sh 2>&1 | tail -1)"  || status=1
exit $status
