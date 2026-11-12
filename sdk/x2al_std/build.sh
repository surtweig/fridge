#!/bin/sh
set -eu

CDPATH= cd "$(dirname "$0")"
../bin/falc vtext.falc vtext.bin --cpm-header
