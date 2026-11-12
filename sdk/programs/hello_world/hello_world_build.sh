#!/bin/sh
set -eu

CDPATH= cd "$(dirname "$0")"
../../bin/cpm --bin hello_world.cpm --include ../../x2al_std --offset 0x0200 --main-return halt
FRIDGE_OPEN_ROM="$PWD/hello_world.bin" exec ../../bin/fridge_emulator
