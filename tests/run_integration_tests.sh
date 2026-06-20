#!/usr/bin/env sh
# Build the gdclip GDExtension and run the GDScript suite inside a real Godot 4
# engine (headless). Point GODOT at a Godot 4 binary (>= 4.2), e.g.:
#
#   GODOT=/path/to/Godot_v4.3-stable_linux.x86_64 tests/run_integration_tests.sh
#
# Requires the godot-cpp submodule to be checked out:
#   git submodule update --init --recursive
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
GODOT=${GODOT:-godot}
JOBS=$(nproc 2>/dev/null || echo 2)

# Build the library the editor/headless engine loads (template_debug).
scons -C "$ROOT" target=template_debug -j"$JOBS"

# Import resources once so the GDExtension is registered, then run the tests.
# `quit(failures)` in the script propagates the failure count as the exit code.
"$GODOT" --headless --path "$ROOT/demo" --import >/dev/null 2>&1 || true
"$GODOT" --headless --path "$ROOT/demo" --script res://tests/run_tests.gd
