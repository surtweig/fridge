#!/bin/sh
set -eu

sdk_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
repo_dir=$(CDPATH= cd "$sdk_dir/.." && pwd)
build_dir="$repo_dir/build-sdk"
bin_dir="$sdk_dir/bin"

mkdir -p "$build_dir"
exec 9>"$build_dir/build.lock"
flock 9

cmake -S "$sdk_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF
cmake --build "$build_dir" --config Debug --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"

# Stage the executables before replacing the installed copies.
mkdir -p "$bin_dir"
stage_dir=$(mktemp -d "$bin_dir/.install.XXXXXX")
trap 'rm -rf "$stage_dir"' EXIT
trap 'exit 1' HUP INT TERM
DESTDIR= cmake --install "$build_dir" --config Debug --prefix "$stage_dir"
mv -f "$stage_dir/bin/falc" "$stage_dir/bin/cpm" "$stage_dir/bin/fridge_emulator" "$bin_dir/"
printf 'Fridge SDK tools installed in %s\n' "$bin_dir"
