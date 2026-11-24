#!/bin/sh
set -eu

CDPATH= cd "$(dirname "$0")"
../bin/cpm --check vegalib.cpm --include ../x2al_std 
