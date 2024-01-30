#!/usr/bin/env bash
set -ex

CHIP_ROOT=$(readlink -f "$(readlink -f "$(dirname "${BASH_SOURCE[0]}")")"/../..)

gn gen --check --fail-on-unused-args --export-compile-commands --root="$CHIP_ROOT"/examples/attribute_database/ "$CHIP_ROOT"/out/linux-x64-attribute-database
ninja -C "$CHIP_ROOT"/out/linux-x64-attribute-database
