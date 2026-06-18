#!/bin/sh
# Host build + run of the gfx renderer unit test (no ESP-IDF, no hardware).
# Asserts pixels and writes gfx_test.ppm. Usage: sh test/host/run.sh
set -e
here=$(dirname "$0")
root="$here/../.."

cc -std=c11 -Wall -Wextra \
   -I"$root/components/ui" \
   -I"$root/components/drivers/display_oled" \
   -I"$here/fakes" \
   "$root/components/ui/gfx.c" "$here/gfx_test.c" \
   -o "$here/gfx_test"

"$here/gfx_test"
