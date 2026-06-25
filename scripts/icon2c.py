#!/usr/bin/env python3
"""Convert image files into 1-bpp C bitmaps for the gfx engine.

Output format (matches a future gfx_blit_1bpp):
  - row-major, top to bottom
  - within each byte, MSB first (bit 7 = leftmost pixel)
  - each row padded to a whole number of bytes (stride = ceil(width / 8))
  - bit = 1  -> draw the pixel (ink), bit = 0 -> leave framebuffer untouched

A source pixel becomes ink (1) when it is dark (grayscale < threshold), which
matches a black drawing on a white background. Use --invert to flip. Fully
transparent pixels (alpha < threshold) are always 0.

Usage:
  python3 scripts/icon2c.py play.png folder.png > icons.h
  python3 scripts/icon2c.py icon.png --invert --threshold 128 -o icon.h
"""

import argparse
import re
import sys
from pathlib import Path

from PIL import Image


def symbol_name(path: Path) -> str:
    """icon_<stem>, with non-alphanumeric runs collapsed to '_'."""
    stem = re.sub(r"[^0-9a-zA-Z]+", "_", path.stem).strip("_").lower()
    if not stem or stem[0].isdigit():
        stem = "img_" + stem
    return "icon_" + stem


def convert(path: Path, threshold: int, invert: bool) -> str:
    img = Image.open(path).convert("RGBA")
    w, h = img.size
    px = img.load()
    stride = (w + 7) // 8

    rows = []
    for y in range(h):
        row = bytearray(stride)
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < threshold:
                ink = False  # transparent -> not drawn
            else:
                lum = (r * 299 + g * 587 + b * 114) // 1000
                ink = lum < threshold
                if invert:
                    ink = not ink
            if ink:
                row[x // 8] |= 0x80 >> (x % 8)
        rows.append(row)

    name = symbol_name(path)
    lines = [
        f"/* {path.name}: {w}x{h}, 1 bpp, MSB-first, stride {stride} bytes/row. */",
        f"#define {name.upper()}_W {w}",
        f"#define {name.upper()}_H {h}",
        f"static const uint8_t {name}[{stride * h}] = {{",
    ]
    for y, row in enumerate(rows):
        hexed = ", ".join(f"0x{byte:02X}" for byte in row)
        comma = "," if y < h - 1 else ""
        lines.append(f"    {hexed}{comma}")
    lines.append("};")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("images", nargs="+", type=Path, help="input image files")
    ap.add_argument("-t", "--threshold", type=int, default=128,
                    help="luminance/alpha cutoff 0..255 (default 128)")
    ap.add_argument("--invert", action="store_true",
                    help="treat light pixels as ink instead of dark")
    ap.add_argument("-o", "--output", type=Path,
                    help="write to this file instead of stdout")
    args = ap.parse_args()

    blocks = ["#pragma once", "#include <stdint.h>", ""]
    for path in args.images:
        blocks.append(convert(path, args.threshold, args.invert))
        blocks.append("")
    text = "\n".join(blocks)

    if args.output:
        args.output.write_text(text)
        print(f"wrote {args.output} ({len(args.images)} icon(s))", file=sys.stderr)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
