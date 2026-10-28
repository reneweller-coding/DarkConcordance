"""Draws DARK CONCORDANCE's icon: a dark sun in eclipse with an amber corona, and under it the Fringe's nine poor
stars on an arc.

Writes res/darkconcordance.ico (16 to 256 px, the Windows program icon), res/darkconcordance.png (512 px) and
src/IconPng.h (a 128 px PNG as bytes: the window's icon on every platform).

    python tools/make_icon.py
"""
import io
import math
import os

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = 2048   # drawn this large, then scaled down smoothly


def icon():
    img = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    # The ground: a rounded square, deep brown-black, lighter round the sun.
    base = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(base).rounded_rectangle([40, 40, S - 40, S - 40], radius=S // 6, fill=(14, 11, 9, 255))
    glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([S * 0.18, S * 0.12, S * 0.82, S * 0.76], fill=(120, 62, 18, 255))
    glow = glow.filter(ImageFilter.GaussianBlur(S * 0.09))
    mask = base.split()[3]
    base = Image.alpha_composite(base, Image.composite(glow, Image.new('RGBA', (S, S)), mask))
    img = Image.alpha_composite(img, base)
    cx, cy, r = S * 0.5, S * 0.44, S * 0.25
    # The corona: a bright ring, blurred into a halo, brightest at the lower right (where the light breaks through).
    corona = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(corona)
    for k, (w, col) in enumerate(((S * 0.075, (200, 110, 30, 160)), (S * 0.035, (240, 160, 60, 220)),
                                  (S * 0.016, (255, 214, 140, 255)))):
        d.ellipse([cx - r - w, cy - r - w, cx + r + w, cy + r + w], fill=col)
    corona = corona.filter(ImageFilter.GaussianBlur(S * 0.012))
    img = Image.alpha_composite(img, Image.composite(corona, Image.new('RGBA', (S, S)), mask))
    flare = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    a = math.radians(40)
    fx, fy = cx + math.cos(a) * r, cy + math.sin(a) * r
    ImageDraw.Draw(flare).ellipse([fx - S * 0.06, fy - S * 0.06, fx + S * 0.06, fy + S * 0.06],
                                  fill=(255, 236, 190, 255))
    flare = flare.filter(ImageFilter.GaussianBlur(S * 0.03))
    img = Image.alpha_composite(img, flare)
    # The dark disc in front.
    ImageDraw.Draw(img).ellipse([cx - r, cy - r, cx + r, cy + r], fill=(10, 8, 7, 255))
    # Nine stars on an arc under it, the middle one the largest.
    stars = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ds = ImageDraw.Draw(stars)
    for i in range(9):
        t = (i - 4) / 4.0
        x = cx + t * S * 0.32
        y = S * 0.83 - (1.0 - t * t) * S * 0.07
        s = S * (0.022 if i == 4 else 0.016 - 0.003 * abs(t))
        ds.ellipse([x - s, y - s, x + s, y + s], fill=(255, 200, 120, 255))
    halo = stars.filter(ImageFilter.GaussianBlur(S * 0.012))
    img = Image.alpha_composite(img, halo)
    img = Image.alpha_composite(img, stars)
    return img


def main():
    big = icon()
    os.makedirs(os.path.join(ROOT, 'res'), exist_ok=True)
    sizes = [16, 24, 32, 48, 64, 128, 256]
    big.resize((256, 256), Image.LANCZOS).save(os.path.join(ROOT, 'res', 'darkconcordance.ico'),
                                               sizes=[(s, s) for s in sizes])
    big.resize((512, 512), Image.LANCZOS).save(os.path.join(ROOT, 'res', 'darkconcordance.png'))
    buf = io.BytesIO()
    big.resize((128, 128), Image.LANCZOS).save(buf, 'PNG', optimize=True)
    data = buf.getvalue()
    lines = ['/**', ' * @file IconPng.h', " * @brief The program's icon as a 128 px PNG (tools/make_icon.py writes it).",
             ' * @author Rene Weller', ' */', '#pragma once', '', '/** @brief The PNG file. */',
             'inline constexpr unsigned char IconPng[] = {']
    for i in range(0, len(data), 20):
        lines.append('    ' + ', '.join(str(b) for b in data[i:i + 20]) + ',')
    lines += ['};', '']
    open(os.path.join(ROOT, 'src', 'IconPng.h'), 'w', newline='\n').write('\n'.join(lines))
    print('icon:', len(data), 'bytes of PNG')


main()
