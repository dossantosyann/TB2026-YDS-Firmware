#!/bin/sh
# Host build + run of the unit tests (no ESP-IDF, no hardware). Usage: sh test/host/run.sh
set -e
here=$(dirname "$0")
root="$here/../.."

# gfx renderer: asserts pixels and writes gfx_test.ppm
cc -std=c11 -Wall -Wextra \
   -I"$root/components/ui" \
   -I"$root/components/drivers/display_oled" \
   -I"$here/fakes" \
   "$root/components/ui/gfx.c" "$here/gfx_test.c" \
   -o "$here/gfx_test"

"$here/gfx_test"

# navigator: asserts push/pop/tick dispatch (pure C, no fakes needed)
cc -std=c11 -Wall -Wextra \
   -I"$root/components/ui" \
   "$root/components/ui/navigator.c" "$here/navigator_test.c" \
   -o "$here/navigator_test"

"$here/navigator_test"

# menu_screen render: asserts highlight + labels, writes menu_screen_test.ppm
cc -std=c11 -Wall -Wextra \
   -I"$root/components/ui" \
   -I"$root/components/screens" \
   -I"$root/components/drivers/display_oled" \
   -I"$here/fakes" \
   "$root/components/ui/gfx.c" "$root/components/ui/navigator.c" \
   "$root/components/screens/menu_screen.c" "$here/menu_screen_test.c" \
   -o "$here/menu_screen_test"

"$here/menu_screen_test"
