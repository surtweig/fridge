#!/bin/sh
set -eu

demo_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
sdk_dir=$(CDPATH= cd "$demo_dir/../.." && pwd)
splash="$sdk_dir/../res/fridge-splash-cropped.png"
converter="$sdk_dir/../tools/png2fridge.py"

cd "$demo_dir"
# The PNG stem determines the CPM identifier prefix.
cp "$splash" splash_columns.png
python3 "$converter" "$splash" splash_rows.rleb --format rle-bitmap
python3 "$converter" splash_columns.png splash_columns.rleb --format rle-bitmap --direction columns

"$sdk_dir/bin/cpm" --bin rledraw_test.cpm --include "$sdk_dir/x2al_std" \
    --include "$sdk_dir/cpm_std" --offset 0x0200 --main-return halt
if [ "${1:-}" = --build-only ]; then
    exit 0
fi

FRIDGE_OPEN_ROM="$demo_dir/rledraw_test.bin" exec "$sdk_dir/bin/fridge_emulator"
