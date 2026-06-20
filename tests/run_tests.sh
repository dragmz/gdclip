#!/usr/bin/env sh
# Build and run the gdclip unit tests with no external test framework.
# Override the compiler with e.g. `CXX=clang++ tests/run_tests.sh`.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
CXX=${CXX:-c++}
OUT="$ROOT/tests/test_gdclip"

"$CXX" -std=c++14 -O2 -Wall -I "$ROOT" \
	"$ROOT/tests/test_gdclip.cpp" \
	"$ROOT/gdclip_core.cpp" \
	"$ROOT/clipper/clipper.cpp" \
	-o "$OUT"

"$OUT"
