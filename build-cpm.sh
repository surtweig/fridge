#!/bin/sh
set -eu

project_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
output_dir="$project_dir/build-cpm"
# Keep the local CMake cache separate from the legacy build files.
build_dir="$output_dir/cmake-build-debug"

cmake -S "$project_dir/emulator/cpm" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build "$build_dir" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"

(
    cd "$build_dir"
    ctest --output-on-failure
)

install -m 755 "$build_dir/cpm" "$output_dir/cpm"
printf 'CPM binary: %s\n' "$output_dir/cpm"
