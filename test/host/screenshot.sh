#!/bin/sh
# Renders every UI screen off-target and exports it as an upscaled PNG for the LaTeX report.
# No ESP-IDF, no hardware -- see screens_snapshot.c and fakes/screens/service_stubs.c.
# Usage: sh test/host/screenshot.sh [scale]   (scale defaults to 4, nearest-neighbor)
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
scale=${1:-4}

cd "$root"
mkdir -p test/host/screenshots docs/screenshots

ui="components/ui"
scr="components/screens"

cc -std=c11 -Wall -Wextra -include stdbool.h \
   -I"$ui" -I"$scr" -I"$scr/audio" -I"$scr/storage" -I"$scr/stats" -I"$scr/settings" \
   -I"components/drivers/display_oled" \
   -I"$here/fakes/screens" \
   -I"components/services/audio" -I"components/services/bluetooth" -I"components/services/power" \
   -I"components/services/settings" -I"components/services/storage" -I"components/services/autonomy" \
   -I"components/services/input" \
   -I"components/bsp" \
   -I"components/drivers/sdcard" -I"components/drivers/gpio_expander" \
   -I"components/drivers/audio_dac" -I"components/drivers/fuel_gauge" \
   -I"$here/fakes" \
   "$ui/gfx.c" "$ui/icons.c" "$ui/navigator.c" "$ui/status_bar.c" \
   "$scr/root_menu.c" \
   "$scr/audio/now_playing.c" "$scr/audio/output_select.c" "$scr/audio/playlist_browser.c" \
   "$scr/storage/storage_screen.c" \
   "$scr/stats/stats_screen.c" "$scr/stats/battery_test.c" "$scr/stats/fuel_gauge_debug.c" \
   "$scr/settings/settings_screen.c" "$scr/settings/bluetooth_settings.c" \
   "$scr/settings/audio_settings.c" "$scr/settings/screen_settings.c" "$scr/settings/power_settings.c" \
   "$here/fakes/screens/service_stubs.c" \
   "$here/screens_snapshot.c" \
   -o "$here/screens_snapshot"

"$here/screens_snapshot"

for ppm in "$root"/test/host/screenshots/*.ppm; do
    [ -e "$ppm" ] || continue
    png="$root/docs/screenshots/$(basename "${ppm%.ppm}").png"
    python3 - "$ppm" "$png" "$scale" <<'PY'
import sys, zlib, struct
src, dst, scale = sys.argv[1], sys.argv[2], int(sys.argv[3])
with open(src, "rb") as f:
    assert f.readline().strip() == b"P6"
    w, h = map(int, f.readline().split()); f.readline()
    data = f.read(w * h * 3)
sw, sh = w * scale, h * scale
rows = bytearray()
for y in range(sh):
    row = bytearray(b"\x00")  # filter byte
    src_y = y // scale
    for x in range(sw):
        src_x = x // scale
        off = (src_y * w + src_x) * 3
        row += data[off:off + 3]
    rows += row
def chunk(t, d):
    c = t + d
    return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
png = (b"\x89PNG\r\n\x1a\n"
       + chunk(b"IHDR", struct.pack(">IIBBBBB", sw, sh, 8, 2, 0, 0, 0))
       + chunk(b"IDAT", zlib.compress(bytes(rows), 9))
       + chunk(b"IEND", b""))
open(dst, "wb").write(png)
PY
    echo "exported $png"
done
