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

# root_menu render: asserts the 3 white icon tiles, writes root_menu_test.ppm
cc -std=c11 -Wall -Wextra \
   -I"$root/components/ui" \
   -I"$root/components/screens" \
   -I"$root/components/drivers/display_oled" \
   -I"$here/fakes" \
   "$root/components/ui/gfx.c" "$root/components/ui/icons.c" \
   "$root/components/screens/root_menu.c" "$here/root_menu_test.c" \
   -o "$here/root_menu_test"

"$here/root_menu_test"

# storage: asserts extension filter (case-insensitive), sort, paths, overflow guard
cc -std=c11 -Wall -Wextra \
   -I"$root/components/services/storage" \
   -I"$root/components/drivers/sdcard" \
   -I"$here/fakes" \
   "$root/components/services/storage/storage.c" "$here/storage_test.c" \
   -o "$here/storage_test"

"$here/storage_test"

# playlist: asserts next/prev/select + wrap, repeat OFF ends, not-ready/empty/oob failing loud,
# rescan clamp, shuffle covering every track once (fake storage index, no SD)
cc -std=c11 -Wall -Wextra \
   -I"$root/components/services/audio" \
   -I"$root/components/services/storage" \
   -I"$here/fakes" \
   "$root/components/services/audio/playlist.c" "$here/playlist_test.c" \
   -o "$here/playlist_test"

"$here/playlist_test"

# input logic: asserts debounce, single event per press, hold/release silent, INOKB ignored,
# button->event mapping (pure logic, no fakes; board_pins.h is plain #defines)
cc -std=c11 -Wall -Wextra \
   -I"$root/components/services/input" \
   -I"$root/components/ui" \
   -I"$root/components/bsp" \
   "$root/components/services/input/input_logic.c" "$here/input_test.c" \
   -o "$here/input_test"

"$here/input_test"
