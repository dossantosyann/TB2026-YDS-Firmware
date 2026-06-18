#!/bin/sh
# Runs the host tests, converts every .ppm they emit to PNG in /tmp, and opens
# them (macOS Preview). The PNGs live in /tmp, so nothing lands in the repo.
# Usage: sh test/host/view.sh
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)

cd "$root"            # the tests write their .ppm to the current dir; pin it to the repo root
sh "$here/run.sh"

for ppm in "$root"/*.ppm; do
    [ -e "$ppm" ] || continue
    png="/tmp/$(basename "${ppm%.ppm}").png"
    python3 - "$ppm" "$png" <<'PY'
import sys, zlib, struct
with open(sys.argv[1], "rb") as f:
    assert f.readline().strip() == b"P6"
    w, h = map(int, f.readline().split()); f.readline()
    data = f.read(w * h * 3)
rows = b"".join(b"\x00" + data[y*w*3:(y+1)*w*3] for y in range(h))
def chunk(t, d):
    c = t + d
    return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
png = (b"\x89PNG\r\n\x1a\n"
       + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
       + chunk(b"IDAT", zlib.compress(rows, 9))
       + chunk(b"IEND", b""))
open(sys.argv[2], "wb").write(png)
PY
    open "$png"
    echo "opened $png"
done
