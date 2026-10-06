#!/bin/bash
# Build the Fridge Assembly Language Compiler (falc) for the host.
#
# falc source belongs to the enclosing Fridge repository. Keep this
# board's compiler cache under .local/falc-build/.
set -euo pipefail

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FRIDGE="$(cd "$TOOLS_DIR/../../.." && pwd)"
BUILD="$TOOLS_DIR/../.local/falc-build"

cmake -S "$FRIDGE/falc" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release

# Upstream links with -static. That needs static libc, which not every host
# has; fall back to a dynamic link rather than failing the build. The flag is
# overridden through the CMake cache, so no upstream file is touched.
if ! cmake --build "$BUILD" -j; then
    echo "==> static link failed, retrying with a dynamic link" >&2
    cmake -S "$FRIDGE/falc" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXE_LINKER_FLAGS=""
    cmake --build "$BUILD" -j
fi

echo "==> $BUILD/falc"
