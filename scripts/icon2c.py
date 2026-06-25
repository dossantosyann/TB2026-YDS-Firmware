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

The icons live in a single translation unit so each bitmap sits once in flash:
generate the definitions into icons.c (default) and the matching extern
declarations into icons.h (--decl), then include icons.h from your screens.

Usage:
  python3 scripts/icon2c.py play.png folder.png -o components/ui/icons.c
  python3 scripts/icon2c.py play.png folder.png --decl -o components/ui/icons.h
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


def render(path: Path, threshold: int, invert: bool):
    """Pack an image into 1-bpp rows. Returns (name, w, h, stride, rows)."""
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

    return symbol_name(path), w, h, stride, rows


def emit_def(path: Path, name: str, w: int, h: int, stride: int, rows) -> str:
    """Definition block for icons.c: the bitmap array with external linkage."""
    lines = [
        f"/* {path.name}: {w}x{h}, 1 bpp, MSB-first, stride {stride} bytes/row. */",
        f"const uint8_t {name}[{stride * h}] = {{",
    ]
    for y, row in enumerate(rows):
        hexed = ", ".join(f"0x{byte:02X}" for byte in row)
        comma = "," if y < h - 1 else ""
        lines.append(f"    {hexed}{comma}")
    lines.append("};")
    return "\n".join(lines)


def emit_decl(path: Path, name: str, w: int, h: int) -> str:
    """Declaration block for icons.h: dimensions plus an extern reference."""
    return "\n".join([
        f"/* {path.name}: {w}x{h}, 1 bpp, MSB-first. */",
        f"#define {name.upper()}_W {w}",
        f"#define {name.upper()}_H {h}",
        f"extern const uint8_t {name}[];",
    ])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("images", nargs="+", type=Path, help="input image files")
    ap.add_argument("-t", "--threshold", type=int, default=128,
                    help="luminance/alpha cutoff 0..255 (default 128)")
    ap.add_argument("--invert", action="store_true",
                    help="treat light pixels as ink instead of dark")
    ap.add_argument("--decl", action="store_true",
                    help="emit a header (extern declarations + W/H) instead of "
                         "the icons.c definitions")
    ap.add_argument("-o", "--output", type=Path,
                    help="write to this file instead of stdout")
    args = ap.parse_args()

    if args.decl:
        blocks = ["#pragma once", "#include <stdint.h>", ""]
    else:
        blocks = ["#include <stdint.h>", ""]
    for path in args.images:
        name, w, h, stride, rows = render(path, args.threshold, args.invert)
        if args.decl:
            blocks.append(emit_decl(path, name, w, h))
        else:
            blocks.append(emit_def(path, name, w, h, stride, rows))
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
