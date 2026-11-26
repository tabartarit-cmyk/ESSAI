#!/usr/bin/env python3
"""Génère l'icône 'globe' (flags/auto.png, 32x32) du menu de langue.

Usage : python3 tools/make_flags.py   (nécessite Pillow)
"""
from pathlib import Path
from PIL import Image, ImageDraw

S = 5
OUT = Path(__file__).resolve().parent.parent / "flags"


def globe():
    g = Image.new("RGBA", (32 * S, 32 * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(g)
    m = 3 * S
    box = [m, m, 32 * S - m, 32 * S - m]
    d.ellipse(box, fill="#2B6CB0")
    line = int(S * 1.1)
    d.ellipse([m, m, 32 * S - m, 32 * S - m], outline="white", width=line)
    d.ellipse([m + 9 * S, m, 32 * S - m - 9 * S, 32 * S - m], outline="white", width=line)
    d.line([16 * S, m, 16 * S, 32 * S - m], fill="white", width=line)
    for y in (11, 16, 21):
        d.line([m, y * S, 32 * S - m, y * S], fill="white", width=line)
    return g.resize((32, 32), Image.LANCZOS)


def main():
    OUT.mkdir(exist_ok=True)
    globe().save(OUT / "auto.png", optimize=True)
    print(f"auto.png -> {OUT}")


if __name__ == "__main__":
    main()
