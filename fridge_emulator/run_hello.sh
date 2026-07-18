#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
FALC="$REPO_ROOT/build-falc/falc"
EMU_DEBUG="$SCRIPT_DIR/build/fridge_emulator"
EMU_RELEASE="$SCRIPT_DIR/build-release/fridge_emulator"
TESTS_DIR="$SCRIPT_DIR/tests"

echo "==> Compiling hello.falc..."
cd "$SCRIPT_DIR"
"$FALC" tests/hello.falc tests/hello.bin

if [ "${1:-}" == "--smoke-test" ]; then
    echo "==> Running smoke test..."
    "$EMU_DEBUG" --smoke-test "$TESTS_DIR/hello.bin"
else
    echo "==> Starting emulator in GUI mode..."
    echo "    (use --smoke-test flag for headless verification)"
    cd "$SCRIPT_DIR"
    FRIDGE_OPEN_ROM="$TESTS_DIR/hello.bin" "$EMU_DEBUG"
fi
