"""Writes the surfaces' colour and ORM maps (assets/textures/*_albedo.png, *_orm.png) once more as DXT1 in a DDS with
all its mip levels beside them: an eighth of the graphics memory of the PNG's (which the card holds uncompressed, with
mips), less to read and nothing to decode when a level loads. The game takes the DDS where there is one
(Scene.cpp, loadTexture). The normal maps stay PNG: DXT1 bends normals visibly, and raylib has no BC5.

    py -3 tools/make_dds.py [--only NAME ...] [--check] [--force]

--check: no files, the mean PSNR of DXT1 against the PNG per kind (how much the compression costs).
"""
import argparse
import glob
import io
import os
import struct
import sys

import numpy
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXTURES = os.path.join(ROOT, "assets", "textures")
KINDS = ("_albedo", "_orm")


def levels(image):
    """The image and its mips down to 1x1 (each half the size, box filtered)."""
    out = [image]
    while image.width > 1 or image.height > 1:
        image = image.resize((max(1, image.width // 2), max(1, image.height // 2)), Image.Resampling.BOX)
        out.append(image)
    return out


def dxt1(image):
    """One level as DXT1 blocks (Pillow's encoder; its own DDS header cut off)."""
    data = io.BytesIO()
    # DXT1 works in 4x4 blocks: the small levels filled out to four with their own pixels (the 1x1 level then is one
    # block of the mean colour, which the game reads for a surface's tint: GameSkin.cpp, surfaceColour).
    w, h = max(4, image.width), max(4, image.height)
    if (w, h) != image.size:
        image = image.resize((w, h), Image.Resampling.NEAREST)
    image.save(data, "DDS", pixel_format="DXT1")
    raw = data.getvalue()
    return raw[128:]


def write_dds(path, image):
    """A DDS with DXT1 and every mip level (written whole: a temporary file renamed)."""
    mips = levels(image.convert("RGB"))
    blocks = [dxt1(m) for m in mips]
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000   # caps, height, width, pixel format, mip count, linear size
    header = struct.pack("<4s7I44x", b"DDS ", 124, flags, image.height, image.width, len(blocks[0]), 0, len(blocks))
    header += struct.pack("<2I4s5I", 32, 0x4, b"DXT1", 0, 0, 0, 0, 0)       # the pixel format: by FourCC
    header += struct.pack("<4I4x", 0x1000 | 0x400000 | 0x8, 0, 0, 0)       # a texture with mips
    part = path + ".part"
    with open(part, "wb") as f:
        f.write(header)
        for b in blocks:
            f.write(b)
    os.replace(part, path)


def psnr(png):
    """DXT1 against the PNG (dB): the top level only."""
    image = Image.open(png).convert("RGB")
    raw = io.BytesIO()
    image.save(raw, "DDS", pixel_format="DXT1")
    back = Image.open(io.BytesIO(raw.getvalue())).convert("RGB")
    a = numpy.asarray(image, dtype=numpy.float64)
    b = numpy.asarray(back, dtype=numpy.float64)
    mse = numpy.mean((a - b) ** 2)
    return 99.0 if mse == 0 else 10.0 * numpy.log10(255.0 ** 2 / mse)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", nargs="*", default=[], help="only these surfaces (names without _albedo/_orm)")
    ap.add_argument("--check", action="store_true", help="only measure the loss (PSNR), write nothing")
    ap.add_argument("--force", action="store_true", help="write again what is there and newer than its PNG")
    a = ap.parse_args()
    pngs = sorted(p for k in KINDS for p in glob.glob(os.path.join(TEXTURES, "*%s.png" % k)))
    if a.only:
        pngs = [p for p in pngs if os.path.basename(p).rsplit("_", 1)[0] in a.only]
    if a.check:
        for k in KINDS:
            values = [psnr(p) for p in pngs if p.endswith(k + ".png")]
            if values:
                print("%-8s %4d maps  PSNR mean %.1f dB, worst %.1f dB" % (k[1:], len(values), sum(values) / len(values),
                                                                           min(values)))
        return
    done = before = after = 0
    for png in pngs:
        dds = png[:-4] + ".dds"
        if not a.force and os.path.exists(dds) and os.path.getmtime(dds) >= os.path.getmtime(png):
            continue
        write_dds(dds, Image.open(png))
        done += 1
        before += os.path.getsize(png)
        after += os.path.getsize(dds)
        if done % 50 == 0:
            print("%d written" % done, flush=True)
    print("%d DDS written (%.0f MB of PNG, %.0f MB of DDS)" % (done, before / 1e6, after / 1e6))


if __name__ == "__main__":
    sys.exit(main())
