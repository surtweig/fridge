#!/bin/sh
set -eu

CDPATH= cd "$(dirname "$0")"
../../bin/cpm --bin vega_test.cpm --include ../../x2al_std --include ../../cpm_std --offset 0x0200 --main-return halt
FRIDGE_OPEN_ROM="$PWD/vega_test.bin" exec ../../bin/fridge_emulator
