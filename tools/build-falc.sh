#!/bin/bash
# Build the Fridge Assembly Language Compiler (falc) for the host.
#
# falc lives in the nested fridge/ clone and is built unmodified into
# fridge/build-falc/, which is the location upstream's own scripts expect.
set -euo pipefail

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FRIDGE="$(cd "$TOOLS_DIR/../fridge" && pwd)"
BUILD="$FRIDGE/build-falc"

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
