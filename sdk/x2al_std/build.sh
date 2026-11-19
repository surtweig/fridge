#!/bin/sh
set -eu

CDPATH= cd "$(dirname "$0")"
../bin/falc vtext.falc vtext.bin --cpm-header
../bin/falc kbrd.falc kbrd.bin --cpm-header
../bin/falc str.falc str.bin --cpm-header
../bin/falc vega.falc vega.bin --cpm-header
../bin/falc rom.falc rom.bin --cpm-header
