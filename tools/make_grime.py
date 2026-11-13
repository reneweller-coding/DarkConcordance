"""Grimes the industrial halls' floors (phase F3): F1's concrete (concretebay, painted by FLUX.2) read as a plain,
washed-out tiled floor; the works keep their character with oil, burns, soot and worn lanes (phase F4: the bay lines
are decals laid by the room, their picture bay_line.png made here). The Mill's halls get the old rusty plating (floor)
sooted, burnt and spattered with slag (foundryplate). Only post-processing of the textures there are, no model: the same seed, the same pictures.

    py -3 tools/make_grime.py
    py -3 tools/make_dds.py --force --only concretebay concretebay_v1 concretebay_v2 foundryplate foundryplate_v1 \
        foundryplate_v2

Writes assets/textures/<name>_albedo.png and _orm.png (the concrete's are rewritten from their FLUX originals in
assets/raw/grime/, kept on the first run); foundryplate's normal map is the plating's.
"""
import os
import shutil

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEX = os.path.join(ROOT, "assets", "textures")
KEEP = os.path.join(ROOT, "assets", "raw", "grime")


def tnoise(size, cells, rng):
    """Tileable value noise (0..1): a grid of cells x cells, bicubic over its wrapped neighbours."""
    g = rng.random((cells, cells)).astype(np.float32)
    big = Image.fromarray(np.tile(g, (3, 3)), mode="F").resize((size * 3, size * 3), Image.Resampling.BICUBIC)
    return np.asarray(big)[size:2 * size, size:2 * size]


def fbm(size, rng, octaves=(4, 8, 16, 32, 64), gain=0.55):
    out, w, total = np.zeros((size, size), np.float32), 1.0, 0.0
    for c in octaves:
        out += tnoise(size, c, rng) * w
        total += w
        w *= gain
    return out / total


def smooth(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def blobs(size, rng, count, r0, r1, ragged):
    """Soft irregular blobs on the torus (each pixel's distance wraps): their cover 0..1."""
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    edge = fbm(size, rng, (8, 16, 32))
    cover = np.zeros((size, size), np.float32)
    for _ in range(count):
        cx, cy = rng.random() * size, rng.random() * size
        r = r0 + (r1 - r0) * rng.random()
        sx, sy = 0.6 + 0.8 * rng.random(), 0.6 + 0.8 * rng.random()
        dx = np.abs(xx - cx)
        dy = np.abs(yy - cy)
        dx = np.minimum(dx, size - dx) / sx
        dy = np.minimum(dy, size - dy) / sy
        d = np.sqrt(dx * dx + dy * dy) / r + (edge - 0.5) * ragged
        cover = np.maximum(cover, smooth(1.0, 0.55, d))
    return cover


def lin(a):
    return np.power(np.clip(a, 0.0, 1.0), 2.2)


def srgb(a):
    return np.power(np.clip(a, 0.0, 1.0), 1.0 / 2.2)


def load(name, kind):
    return np.asarray(Image.open(os.path.join(TEX, "%s_%s.png" % (name, kind))).convert("RGB")).astype(np.float32) / 255.0


def save(name, kind, rgb):
    Image.fromarray((np.clip(rgb, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)).save(
        os.path.join(TEX, "%s_%s.png" % (name, kind)))


def dilate(mask, r):
    """A max filter of radius r on the torus (the lines widen across the tile's edge too)."""
    out = mask.copy()
    for d in range(1, r + 1):
        for ax in (0, 1):
            out = np.maximum(out, np.roll(mask, d, ax))
            out = np.maximum(out, np.roll(mask, -d, ax))
    return out


def concrete(name, seed):
    """Darker concrete with its joints grimed, worn lanes, oil, tyre marks, burns, and bold chipped bay lines."""
    for kind in ("albedo", "orm"):
        keep = os.path.join(KEEP, "%s_%s.png" % (name, kind))
        if not os.path.exists(keep):
            shutil.copyfile(os.path.join(TEX, "%s_%s.png" % (name, kind)), keep)
    a = np.asarray(Image.open(os.path.join(KEEP, name + "_albedo.png")).convert("RGB")).astype(np.float32) / 255.0
    orm = np.asarray(Image.open(os.path.join(KEEP, name + "_orm.png")).convert("RGB")).astype(np.float32) / 255.0
    size = a.shape[0]
    rng = np.random.default_rng(seed)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    # The old thin yellow lines: found by their colour, painted over in concrete, then laid again bold along them.
    yellow = ((r - b > 0.16) & (g - b > 0.1)).astype(np.float32)
    grey = a.mean(axis=2, keepdims=True)
    base = np.where(yellow[..., None] > 0, np.full_like(a, float(np.median(grey))), a)
    base = base * 0.35 + base.mean(axis=2, keepdims=True) * 0.65   # nearly grey
    # Grime over everything: dark mottling at several scales, soot-brown in the lows.
    dirt = fbm(size, rng)
    grain = fbm(size, rng, (64, 128, 256))
    tone = (0.62 + 0.3 * smooth(0.2, 0.8, dirt)) * (0.85 + 0.3 * grain)
    col = base * tone[..., None] * np.array([1.0, 0.97, 0.92], np.float32)
    # The joints darker (grime gathers there): where the picture is darker than its surroundings.
    blur = np.asarray(Image.fromarray((grey[..., 0] * 255).astype(np.uint8)).resize((size // 8, size // 8),
                                      Image.Resampling.BOX).resize((size, size), Image.Resampling.BILINEAR),
                      np.float32) / 255.0
    joint = smooth(0.03, 0.12, blur - grey[..., 0])
    col *= (1.0 - 0.55 * joint)[..., None]
    # Worn lanes: long strips along one axis, lighter and smoother (forklifts, boots).
    lane = tnoise(size, 3, rng)
    lane = np.asarray(Image.fromarray(lane).resize((size, size)), np.float32)
    lanes = smooth(0.55, 0.8, np.tile(tnoise(size, 6, rng)[:, :1], (1, size)) * 0.6 + lane * 0.4)
    col = col * (1.0 + 0.35 * lanes[..., None])
    # Tyre marks: faint dark streaks in pairs along the lanes, coming and going.
    yy = np.mgrid[0:size, 0:size][0].astype(np.float32)
    tyres = np.zeros((size, size), np.float32)
    for _ in range(2):
        y0 = rng.random() * size
        for off in (-20.0, 20.0):
            d = np.abs(yy - y0 - off)
            d = np.minimum(d, size - d)
            tyres = np.maximum(tyres, smooth(7.0, 2.0, d))
    tyres *= smooth(0.45, 0.7, tnoise(size, 4, rng)) * (0.5 + 0.5 * grain)
    col *= (1.0 - 0.35 * tyres)[..., None]
    # Oil: dark, a little brown-blue, smooth (its roughness low).
    oil = blobs(size, rng, 3, size * 0.04, size * 0.1, 1.6) * (0.6 + 0.4 * grain)
    col = col * (1.0 - 0.6 * oil[..., None]) + oil[..., None] * np.array([0.012, 0.012, 0.016], np.float32)
    # Burns: black soot fading out, a brown ring round some.
    burn = blobs(size, rng, 1, size * 0.06, size * 0.1, 1.8) * smooth(0.3, 0.6, grain + 0.2)
    ring = np.clip(burn * (1.0 - burn) * 4.0, 0.0, 1.0)
    col = col * (1.0 - 0.8 * burn[..., None]) + ring[..., None] * np.array([0.05, 0.03, 0.015], np.float32)
    # (Phase F4: no lines in the picture: they came from the FLUX picture's old lines and lay over every hall as a grid;
    # the halls' bay lines are decals laid by the room, Level::bayLines, their picture bay_line() below.)
    rough = orm[..., 1] * (1.0 - 0.6 * oil) * (1.0 - 0.15 * lanes) + 0.05 * burn
    ao = orm[..., 0] * (1.0 - 0.3 * joint)
    out_orm = np.stack([ao, np.clip(rough, 0.08, 1.0), np.zeros_like(ao)], axis=2)
    save(name, "albedo", col)
    save(name, "orm", out_orm)
    print(name, "mean linear %.3f" % lin(col).mean())


def foundry(src, name, seed):
    """The rusty plating with its yellow markings, sooted, burnt in blue-brown rings, spattered with slag."""
    a = load(src, "albedo")
    orm = load(src, "orm")
    size = a.shape[0]
    rng = np.random.default_rng(seed)
    soot = smooth(0.4, 0.8, fbm(size, rng))
    col = a * (1.0 - 0.45 * soot[..., None]) * 0.9
    # Heat: tempered rings (straw, brown, blue) round hot spots, the middle dark.
    heat = blobs(size, rng, 3, size * 0.06, size * 0.12, 1.4)
    band = np.clip(heat * (1.0 - heat) * 4.0, 0.0, 1.0)
    tint = np.where((heat < 0.5)[..., None], np.array([0.55, 0.38, 0.2], np.float32),
                    np.array([0.22, 0.24, 0.42], np.float32))
    col = col * (1.0 - 0.5 * band[..., None]) + band[..., None] * tint * col.mean(axis=2, keepdims=True) * 1.2
    col *= (1.0 - 0.35 * smooth(0.6, 1.0, heat))[..., None]
    # Slag: small dark nodules with a rusty edge, in spatters.
    spatter = smooth(0.55, 0.75, tnoise(size, 6, rng))
    grains = smooth(0.8, 0.9, tnoise(size, 96, rng)) * spatter
    col = col * (1.0 - 0.7 * grains[..., None]) + grains[..., None] * np.array([0.035, 0.022, 0.015], np.float32)
    # Oil.
    oil = blobs(size, rng, 2, size * 0.03, size * 0.07, 1.6)
    col = col * (1.0 - 0.45 * oil[..., None])
    rough = np.clip(orm[..., 1] + 0.2 * soot - 0.4 * oil + 0.1 * grains, 0.1, 1.0)
    metal = orm[..., 2] * (1.0 - 0.5 * soot) * (1.0 - 0.6 * grains)
    save(name, "albedo", col)
    save(name, "orm", np.stack([orm[..., 0], rough, metal], axis=2))
    shutil.copyfile(os.path.join(TEX, src + "_normal.png"), os.path.join(TEX, name + "_normal.png"))
    print(name, "mean linear %.3f" % lin(col).mean())


def hulldeck(src, name, seed):
    """The hull's white plates as a shipyard's deck under the sun (phase F4: at 0.52 they shone white in the Werft's
    yard): darker, grimed, scuffed lighter where boots and carts go, oil, welding burns."""
    a = load(src, "albedo")
    orm = load(src, "orm")
    size = a.shape[0]
    rng = np.random.default_rng(seed)
    dirt = fbm(size, rng)
    grain = fbm(size, rng, (64, 128, 256))
    col = a * 0.5 * ((0.62 + 0.38 * smooth(0.2, 0.8, dirt)) * (0.85 + 0.3 * grain))[..., None]
    col *= np.array([1.0, 0.97, 0.9], np.float32)
    lanes = smooth(0.55, 0.8, np.tile(tnoise(size, 5, rng)[:, :1], (1, size)) * 0.6 + tnoise(size, 3, rng) * 0.4)
    col *= (1.0 + 0.3 * lanes)[..., None]
    oil = blobs(size, rng, 2, size * 0.05, size * 0.1, 2.0) * (0.5 + 0.5 * grain)
    col = col * (1.0 - 0.45 * oil[..., None])
    burn = blobs(size, rng, 1, size * 0.05, size * 0.09, 1.8)
    ring = np.clip(burn * (1.0 - burn) * 4.0, 0.0, 1.0)
    col = col * (1.0 - 0.45 * burn[..., None]) + ring[..., None] * np.array([0.04, 0.035, 0.06], np.float32)
    rough = np.clip(orm[..., 1] + 0.15 * (1.0 - dirt) - 0.35 * oil - 0.1 * lanes, 0.1, 1.0)
    save(name, "albedo", col)
    save(name, "orm", np.stack([orm[..., 0], rough, orm[..., 2] * (1.0 - 0.5 * oil)], axis=2))
    shutil.copyfile(os.path.join(TEX, src + "_normal.png"), os.path.join(TEX, name + "_normal.png"))
    print(name, "mean linear %.3f" % lin(col).mean())


def bay_line():
    """A painted bay line, 2 m by 12 cm (512 by 32): safety yellow, worn and chipped, its ends meeting the next
    piece's (the noise wraps along it), its long edges a little ragged; assets/textures/decals/bay_line.png."""
    rng = np.random.default_rng(91)
    w, h = 512, 32
    chips = fbm(w, rng, (8, 16, 32, 64, 128))[:h, :]
    dirt = fbm(w, rng, (4, 8, 16))[:h, :]
    rag = fbm(w, rng, (32, 64))[0, :]
    yy = np.mgrid[0:h, 0:w][0].astype(np.float32)
    half = 11.0 + 3.0 * (rag - 0.5)
    edge = smooth(half + 1.5, half - 1.5, np.abs(yy - (h - 1) * 0.5))
    cover = edge * smooth(0.3, 0.42, chips) * 0.92
    col = np.array([0.86, 0.64, 0.1], np.float32) * (0.7 + 0.4 * dirt[..., None])
    rgba = np.concatenate([col, cover[..., None]], axis=2)
    Image.fromarray((np.clip(rgba, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), "RGBA").save(
        os.path.join(TEX, "decals", "bay_line.png"))
    print("bay_line cover %.2f" % cover.mean())


def main():
    os.makedirs(KEEP, exist_ok=True)
    bay_line()
    for i, n in enumerate(("concretebay", "concretebay_v1", "concretebay_v2")):
        concrete(n, 31 + i)
    for i, (s, n) in enumerate((("floor", "foundryplate"), ("floor_v1", "foundryplate_v1"),
                                ("floor_v2", "foundryplate_v2"))):
        foundry(s, n, 71 + i)
    for i, (s, n) in enumerate((("hullplate", "hulldeck"), ("hullplate_v1", "hulldeck_v1"),
                                ("hullplate_v2", "hulldeck_v2"))):
        hulldeck(s, n, 81 + i)


if __name__ == "__main__":
    main()
