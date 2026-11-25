#!/usr/bin/env python3
"""Génère les drapeaux (flags/<code langue>.png, 48x32) et l'icône 'auto' (globe).

Les drapeaux sont dessinés de façon simplifiée (sans armoiries ni textes) pour rester lisibles
en petite taille. Le fichier porte le code de la LANGUE ; le dessin est le drapeau du pays choisi pour cette langue :
  en GB | fr FR | es ES | de DE | it IT | pt BR | nl NL | pl PL | ru RU | uk UA | tr TR | ar AE | he IL | ja JP | zh CN | ko KR
Pour changer ou ajouter un drapeau : déposer simplement un fichier <code>.png dans flags/.
Usage : python3 tools/make_flags.py   (nécessite Pillow)
"""
import math
from pathlib import Path
from PIL import Image, ImageDraw

S = 5
W, H = 48 * S, 32 * S
OUT = Path(__file__).resolve().parent.parent / "flags"


def new(bg):
    im = Image.new("RGB", (W, H), bg)
    return im, ImageDraw.Draw(im)


def stripes_h(colors, weights=None):
    weights = weights or [1] * len(colors)
    im, d = new(colors[0])
    tot, y = sum(weights), 0
    for c, w in zip(colors, weights):
        h = round(H * w / tot)
        d.rectangle([0, y, W, y + h], fill=c)
        y += h
    return im


def stripes_v(colors):
    im, d = new(colors[0])
    for i, c in enumerate(colors):
        d.rectangle([round(W * i / len(colors)), 0, round(W * (i + 1) / len(colors)), H], fill=c)
    return im


def star(d, cx, cy, r, fill, rot=-90, inner=0.382):
    pts = []
    for i in range(10):
        a = math.radians(rot + i * 36)
        rr = r if i % 2 == 0 else r * inner
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    d.polygon(pts, fill=fill)


def gb():
    im, d = new("#012169")
    d.line([0, 0, W, H], fill="white", width=int(H * 0.22))
    d.line([0, H, W, 0], fill="white", width=int(H * 0.22))
    d.line([0, 0, W, H], fill="#C8102E", width=int(H * 0.08))
    d.line([0, H, W, 0], fill="#C8102E", width=int(H * 0.08))
    d.rectangle([W / 2 - H * 0.17, 0, W / 2 + H * 0.17, H], fill="white")
    d.rectangle([0, H / 2 - H * 0.17, W, H / 2 + H * 0.17], fill="white")
    d.rectangle([W / 2 - H * 0.1, 0, W / 2 + H * 0.1, H], fill="#C8102E")
    d.rectangle([0, H / 2 - H * 0.1, W, H / 2 + H * 0.1], fill="#C8102E")
    return im


def br():
    im, d = new("#009C3B")
    d.polygon([(W / 2, H * 0.11), (W * 0.915, H / 2), (W / 2, H * 0.89), (W * 0.085, H / 2)], fill="#FFDF00")
    r = H * 0.25
    box = [W / 2 - r, H / 2 - r, W / 2 + r, H / 2 + r]
    d.ellipse(box, fill="#002776")
    band = Image.new("L", (W, H), 0)
    bd = ImageDraw.Draw(band)
    bd.arc([W / 2 - r * 1.7, H / 2 - r * 0.2, W / 2 + r * 1.7, H / 2 + r * 2.6], 195, 345, fill=255, width=int(H * 0.05))
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).ellipse(box, fill=255)
    from PIL import ImageChops
    band = ImageChops.multiply(band, mask)
    im.paste("white", (0, 0), band)
    return im


def tr():
    im, d = new("#E30A17")
    cx, cy = W * 0.36, H / 2
    d.ellipse([cx - H * 0.25, cy - H * 0.25, cx + H * 0.25, cy + H * 0.25], fill="white")
    d.ellipse([cx - H * 0.2 + H * 0.06, cy - H * 0.2, cx + H * 0.2 + H * 0.06, cy + H * 0.2], fill="#E30A17")
    star(d, W * 0.57, cy, H * 0.13, "white", rot=-90 + 18)
    return im


def ae():
    im, d = new("white")
    d.rectangle([W * 0.25, 0, W, H / 3], fill="#00732F")
    d.rectangle([W * 0.25, H / 3, W, 2 * H / 3], fill="white")
    d.rectangle([W * 0.25, 2 * H / 3, W, H], fill="black")
    d.rectangle([0, 0, W * 0.25, H], fill="#FF0000")
    return im


def il():
    im, d = new("white")
    blue = "#0038B8"
    d.rectangle([0, H * 0.09, W, H * 0.09 + H * 0.12], fill=blue)
    d.rectangle([0, H * 0.79 - H * 0.0, W, H * 0.79 + H * 0.12], fill=blue)
    cx, cy, r = W / 2, H / 2, H * 0.2
    t1 = [(cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))) for a in (-90, 30, 150)]
    t2 = [(cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))) for a in (90, 210, 330)]
    d.line(t1 + [t1[0]], fill=blue, width=int(H * 0.04), joint="curve")
    d.line(t2 + [t2[0]], fill=blue, width=int(H * 0.04), joint="curve")
    return im


def jp():
    im, d = new("white")
    r = H * 0.3
    d.ellipse([W / 2 - r, H / 2 - r, W / 2 + r, H / 2 + r], fill="#BC002D")
    return im


def cn():
    im, d = new("#DE2910")
    star(d, W / 6, H / 4, H * 0.15, "#FFDE00")
    big = (W / 6, H / 4)
    for (px, py) in [(10 / 30, 2 / 20), (12 / 30, 4 / 20), (12 / 30, 7 / 20), (10 / 30, 9 / 20)]:
        x, y = W * px, H * py
        rot = math.degrees(math.atan2(big[1] - y, big[0] - x))
        star(d, x, y, H * 0.05, "#FFDE00", rot=rot)
    return im


def kr():
    im, d = new("white")
    cx, cy, r = W / 2, H / 2, H * 0.25
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill="#0047A0")
    d.pieslice([cx - r, cy - r, cx + r, cy + r], 180, 360, fill="#CD2E3A")
    d.ellipse([cx - r, cy - r / 2, cx, cy + r / 2], fill="#CD2E3A")
    d.ellipse([cx, cy - r / 2, cx + r, cy + r / 2], fill="#0047A0")

    def trigram(angle, broken):
        t = Image.new("RGBA", (int(H * 0.3), int(H * 0.26)), (0, 0, 0, 0))
        td = ImageDraw.Draw(t)
        bh = t.size[1] / 5.0
        for i, br_ in enumerate(broken):
            y = i * bh * 2
            if br_:
                td.rectangle([0, y, t.size[0] * 0.42, y + bh], fill="black")
                td.rectangle([t.size[0] * 0.58, y, t.size[0], y + bh], fill="black")
            else:
                td.rectangle([0, y, t.size[0], y + bh], fill="black")
        t = t.rotate(angle, expand=True, resample=Image.BICUBIC)
        return t

    # (x, y, rotation, [barre haute, milieu, basse] cassée ?)
    d_x, d_y = W * 0.2, H * 0.27
    for sx, sy, ang, pat in [(-1, -1, 56, (0, 0, 0)), (1, -1, -56, (1, 1, 1)), (-1, 1, -56, (0, 1, 0)), (1, 1, 56, (1, 0, 1))]:
        t = trigram(ang, pat)
        im.paste(t, (int(cx + sx * d_x - t.size[0] / 2), int(cy + sy * d_y - t.size[1] / 2)), t)
    return im


def es():
    return stripes_h(["#AA151B", "#F1BF00", "#AA151B"], [1, 2, 1])


FLAGS = {
    "en": gb,
    "fr": lambda: stripes_v(["#0055A4", "#FFFFFF", "#EF4135"]),
    "es": es,
    "de": lambda: stripes_h(["#000000", "#DD0000", "#FFCE00"]),
    "it": lambda: stripes_v(["#008C45", "#FFFFFF", "#CD212A"]),
    "pt": br,
    "nl": lambda: stripes_h(["#AE1C28", "#FFFFFF", "#21468B"]),
    "pl": lambda: stripes_h(["#FFFFFF", "#DC143C"]),
    "ru": lambda: stripes_h(["#FFFFFF", "#0039A6", "#D52B1E"]),
    "uk": lambda: stripes_h(["#0057B7", "#FFD700"]),
    "tr": tr,
    "ar": ae,
    "he": il,
    "ja": jp,
    "zh": cn,
    "ko": kr,
}


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
    for code, fn in FLAGS.items():
        im = fn()
        d = ImageDraw.Draw(im)
        d.rectangle([0, 0, W - 1, H - 1], outline="#9A9A9A", width=S)   # liseré : visible sur fond blanc
        im.resize((48, 32), Image.LANCZOS).save(OUT / f"{code}.png", optimize=True)
    globe().save(OUT / "auto.png", optimize=True)
    print(f"{len(FLAGS)} drapeaux + auto.png -> {OUT}")


if __name__ == "__main__":
    main()
