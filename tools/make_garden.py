"""Derives the gardens' surfaces from textures there are, the leaves from two pictures painted locally:

    G:\\Tools\\ImageGeneration\\venv\\Scripts\\python.exe tools\\make_garden.py [--local]
    py -3 tools/make_dds.py --only lawn lawn_v1 lawn_v2 foliage bark

--local paints the leaves' pictures that are missing (assets/raw/garden/leafcard.jpg, foliage.jpg; FLUX.2 in WSL2, as
make_textures.py --local): large broad leaves in calm clumps, as a painted game's crowns (13.11.2026; before, both
were cut from the hedge's fine boxwood, which speckled restlessly from the game's height). Without them the old
derivation from the hedge.

  lawn      grass with most of its daisies gone (from the camera's height they flickered as white specks), a shade
            darker and calmer
  lawn_v1   grass_v1 the same way
  lawn_v2   the lawn turned and a little drier (grass_v2 has a path of stepping stones across it, which the patchwork
            laid everywhere as diagonal stripes)
  foliage   the hedge's leaves without their colour: the plants tint it (teal, violet, rust, ...)
  bark      dustrock stood upright (its layers run up a trunk) and greyed
  boulder   dustrock greyed and lightened: the stone of the gardens' rocky edges
  blades    a tuft of long grass blades (alpha: cut out), scattered over the lawn
  leafcard  a cluster of the foliage's leaves cut out with a ragged rim, round the crowns; an atlas of four clumps
            made of fans of it (leaf_atlas, 13.11.2026: one rosette on every card read as blossom)
  path_1/2  decals of trodden earth with gravel, their rims fading (assets/textures/decals)

Each writes <name>_albedo.png, _orm.png and _normal.png into assets/textures (blades and leafcard keep their alpha:
no DDS for them, DXT1 would cut it).
"""
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXTURES = os.path.join(ROOT, "assets", "textures")
PAINTED = os.path.join(ROOT, "assets", "raw", "garden")
# The leaves painted locally (paint_jobs): a cluster cut out of white for the cards, a seamless canopy for the cores.
PAINT = {
    "leafcard": ("A single round cluster of about ten large broad tree leaves seen from straight above, overlapping in "
                 "two or three calm soft groups, simple painted leaf shapes with a lit upper side and gently darker "
                 "leaves beneath, hand-painted fantasy game art, soft brush strokes, low contrast, green, isolated in "
                 "the centre, nothing else, on a plain pure white background, flat even lighting, no text, no frame"),
    "foliage": ("Seamless tileable texture, top-down view of a dense tree canopy of large broad leaves gathered in soft "
                "rounded clumps, hand-painted fantasy game texture, calm grouping of light and dark clumps, low "
                "contrast, green, flat even lighting, no perspective, no text, highly detailed"),
}
KINDS = ("albedo", "orm", "normal")
KEEP = 0.12          # share of the daisies kept (in patches)
OFFSETS = [(37, 211), (-163, 91), (233, -57), (-89, -173), (151, 149), (-211, 23)]


def load(name, kind):
    return np.asarray(Image.open(os.path.join(TEXTURES, "%s_%s.png" % (name, kind))).convert("RGB"),
                      dtype=np.float32) / 255.0


def save(a, name, kind):
    Image.fromarray(np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8)).save(
        os.path.join(TEXTURES, "%s_%s.png" % (name, kind)))


def blur(a, radius):
    """A box blur, three times (near a Gaussian), wrapping round (the tiles are seamless)."""
    for _ in range(3):
        for axis in (0, 1):
            s = np.zeros_like(a)
            for k in range(-radius, radius + 1):
                s += np.roll(a, k, axis=axis)
            a = s / (2 * radius + 1)
    return a


def daisies(albedo):
    """Where the flowers are: white petals and yellow hearts, grown a little; a few patches keep theirs."""
    r, g, b = albedo[..., 0], albedo[..., 1], albedo[..., 2]
    white = (np.minimum(np.minimum(r, g), b) > 0.42) & (np.max(albedo, axis=2) - np.min(albedo, axis=2) < 0.28)
    yellow = (r > 0.5) & (g > 0.42) & (b < 0.38) & (r - b > 0.25)
    mask = (white | yellow).astype(np.float32)
    mask = blur(mask, 2) > 0.05
    h, w = mask.shape
    rng = np.random.default_rng(7)
    cells = rng.random((h // 128 + 1, w // 128 + 1)) < KEEP
    keep = np.kron(cells, np.ones((128, 128), dtype=bool))[:h, :w]
    return mask & ~keep


def fill(a, mask):
    """Fills the masked pixels from the same tile shifted (grass from elsewhere), the rest from the blurred grass."""
    out = a.copy()
    todo = mask.copy()
    for dy, dx in OFFSETS:
        src = np.roll(a, (dy, dx), axis=(0, 1))
        ok = todo & ~np.roll(mask, (dy, dx), axis=(0, 1))
        out[ok] = src[ok]
        todo &= ~ok
    if todo.any():
        weight = blur((~mask).astype(np.float32)[..., None], 6)
        smooth = blur(a * (~mask)[..., None], 6) / np.maximum(weight, 1e-3)
        out[todo] = smooth[todo]
    return out


def lawn(src, dst):
    maps = {k: load(src, k) for k in KINDS}
    mask = daisies(maps["albedo"])
    for k in KINDS:
        maps[k] = fill(maps[k], mask)
    a = maps["albedo"]
    grey = a.mean(axis=2, keepdims=True)
    # Calmer and towards olive, as an old meadow rather than a sports lawn.
    maps["albedo"] = (grey + (a - grey) * 0.8) * np.array([0.84, 0.78, 0.56], dtype=np.float32)
    for k in KINDS:
        save(maps[k], dst, k)
    return maps


def turn_normal(n, quarter):
    """A normal map turned by quarter turns counter-clockwise (OpenGL: red right, green up)."""
    v = n * 2.0 - 1.0
    for _ in range(quarter % 4):
        v = np.rot90(v, 1, axes=(0, 1))
        v = np.stack([-v[..., 1], v[..., 0], v[..., 2]], axis=2)
    return (v + 1.0) * 0.5


def main():
    base = lawn("grass", "lawn")
    lawn("grass_v1", "lawn_v1")
    # Drier patches: the plain lawn half turned, a trace yellower and darker (more, and the cells' squares show).
    dry = np.rot90(base["albedo"], 2, axes=(0, 1)) * np.array([1.02, 0.99, 0.9], dtype=np.float32) * 0.96
    save(dry, "lawn_v2", "albedo")
    save(np.rot90(base["orm"], 2, axes=(0, 1)), "lawn_v2", "orm")
    save(turn_normal(base["normal"], 2), "lawn_v2", "normal")

    # Foliage: the painted canopy's (else the hedge's) luminance, brightened so that a tint gives the colour.
    leaf = load("hedge", "albedo")
    lum = leaf @ np.array([0.3, 0.55, 0.15], dtype=np.float32)
    lum = lum / max(float(lum.mean()), 1e-3) * 0.55
    if not painted_foliage():
        save(np.repeat(lum[..., None], 3, axis=2), "foliage", "albedo")
        save(load("hedge", "orm"), "foliage", "orm")
        save(load("hedge", "normal"), "foliage", "normal")

    # Bark: dustrock upright, nearly grey (a warm trace left), darker.
    rock = np.rot90(load("dustrock", "albedo"), 1, axes=(0, 1))
    grey = rock.mean(axis=2, keepdims=True)
    bark = (grey + (rock - grey) * 0.25) * 0.8
    save(bark, "bark", "albedo")
    save(np.rot90(load("dustrock", "orm"), 1, axes=(0, 1)), "bark", "orm")
    save(turn_normal(load("dustrock", "normal"), 1), "bark", "normal")

    # Boulders: dustrock greyed to a pale, weathered stone with soft layers (the cave rock's frost and fossils do not
    # belong in a garden).
    stone = load("dustrock", "albedo")
    grey = stone.mean(axis=2, keepdims=True)
    cool = np.array([0.97, 1.0, 1.04], dtype=np.float32)
    save(np.clip(((grey + (stone - grey) * 0.06) * 1.1 + 0.05) * cool, 0, 1), "boulder", "albedo")
    save(load("dustrock", "orm"), "boulder", "orm")
    save(load("dustrock", "normal"), "boulder", "normal")

    blades()
    if not painted_leafcard():
        leafcard(lum)
    needles()
    paths()


def paint_jobs():
    """The leaves' pictures not painted yet: [(raw path, aspect, prompt, seed)] for make_textures' local painting."""
    return [(os.path.join(PAINTED, name + ".jpg"), "1:1", prompt, 1) for name, prompt in PAINT.items()
            if not os.path.exists(os.path.join(PAINTED, name + ".jpg"))]


def calm(lum, keep=0.55, group=0.35, radius=24):
    """A leaf picture's luminance made calm: its fine contrast cut to `keep`, a share `group` of it from a wide blur
    (the clumps' light and dark read as wholes), its mean at 0.55 (the plants' tint gives the colour)."""
    mean = float(lum.mean())
    fine = mean + (lum - mean) * keep
    wide = blur(lum[..., None], radius)[..., 0]
    out = fine * (1.0 - group) + wide * group
    return out / max(float(out.mean()), 1e-3) * 0.55


def painted_foliage():
    """The crowns' cores from the painted canopy: made seamless, its luminance calmed, its bumps soft."""
    raw = os.path.join(PAINTED, "foliage.jpg")
    if not os.path.exists(raw):
        return False
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import make_textures
    make_textures.tile_maps("foliage", raw, 0.0, 0.85, 2.0)
    a = load("foliage", "albedo")
    lum = calm(a @ np.array([0.3, 0.55, 0.15], dtype=np.float32), keep=0.6, group=0.3, radius=20)
    save(np.repeat(np.clip(lum, 0, 1)[..., None], 3, axis=2), "foliage", "albedo")
    return True


def painted_leafcard():
    """The leaf cards from the painted cluster: keyed out of its white, the leaves' grey calmed, the outline the
    leaves' own (alpha), a clear rim fading so that no card shows a square edge."""
    raw = os.path.join(PAINTED, "leafcard.jpg")
    if not os.path.exists(raw):
        return False
    img = np.asarray(Image.open(raw).convert("RGB").resize((512, 512), Image.Resampling.LANCZOS),
                     dtype=np.float32) / 255.0
    dist = (1.0 - img).max(axis=2)
    alpha = np.clip((dist - 0.1) / 0.25, 0.0, 1.0)
    colour = np.clip((img - (1.0 - dist[..., None])) / np.maximum(dist, 0.1)[..., None], 0.0, 1.0)
    lum = colour @ np.array([0.3, 0.55, 0.15], dtype=np.float32)
    inside = alpha > 0.5
    mean = float(lum[inside].mean()) if inside.any() else 0.5
    lum = np.where(inside, lum, mean)
    lum = calm(lum, keep=0.6, group=0.3, radius=16)
    # No leaf reaches the card's edge.
    yy, xx = np.mgrid[0:512, 0:512].astype(np.float32)
    edge = np.clip(np.minimum(np.minimum(xx, 511 - xx), np.minimum(yy, 511 - yy)) / 20.0, 0.0, 1.0)
    rgb = np.repeat(np.clip(lum * 1.1, 0, 1)[..., None], 3, axis=2)
    save_rgba(leaf_atlas(np.concatenate([rgb, (alpha * edge)[..., None]], axis=2)),
              os.path.join(TEXTURES, "leafcard_albedo.png"))
    flat_maps("leafcard")
    return True


# The atlas's four clusters (phase F2: every card was the same star-shaped rosette, which repeated and read like
# blossom): each three or four fans cut out of the painted cluster (a sector of its leaves, soft at its sides), scaled,
# turned and laid off the quadrant's middle, the ones below darker: an uneven clump, no star. Each fan: x, y in the
# quadrant, scale, degrees turned, the sector's middle and width (degrees), shade.
ATLAS = [
    [(0.58, 0.6, 0.78, 75.0, 200.0, 230.0, 0.7), (0.4, 0.5, 0.8, 20.0, 40.0, 240.0, 0.84),
     (0.56, 0.4, 0.74, 160.0, 300.0, 210.0, 1.0)],
    [(0.62, 0.42, 0.74, 140.0, 90.0, 240.0, 0.72), (0.38, 0.44, 0.76, 10.0, 250.0, 220.0, 0.82),
     (0.5, 0.54, 0.8, 260.0, 0.0, 250.0, 1.0)],
    [(0.45, 0.58, 0.82, 200.0, 120.0, 250.0, 0.72), (0.58, 0.44, 0.74, 35.0, 330.0, 220.0, 0.88),
     (0.38, 0.4, 0.6, 300.0, 60.0, 200.0, 1.0), (0.64, 0.64, 0.56, 110.0, 210.0, 200.0, 0.95)],
    [(0.44, 0.6, 0.78, 300.0, 170.0, 240.0, 0.72), (0.58, 0.5, 0.84, 51.0, 20.0, 230.0, 0.86),
     (0.44, 0.4, 0.62, 230.0, 280.0, 210.0, 1.0)],
]


def leaf_atlas(card):
    """The leaf card (RGBA, 512 square) as an atlas of four different clusters in a 1024 square (Level's leaves() picks
    a quadrant per card): fans of its leaves laid over each other, a clear rim round each quadrant (the coarser levels'
    filtering must not carry a neighbour's leaves over)."""
    card = card.copy()
    # Calmer: the lit upper leaves nearer the ones below (their contrast drew the star).
    inside = card[..., 3] > 0.5
    mean = card[..., :3][inside].mean(axis=0) if inside.any() else np.array([0.5, 0.5, 0.5], dtype=np.float32)
    card[..., :3] = mean + (card[..., :3] - mean) * 0.7
    yy, xx = np.mgrid[0:512, 0:512].astype(np.float32)
    angle = np.degrees(np.arctan2(yy - 255.5, xx - 255.5))
    radius = np.sqrt((xx - 255.5) ** 2 + (yy - 255.5) ** 2) / 256.0
    out = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    rim = np.clip(np.minimum(np.minimum(xx, 511 - xx), np.minimum(yy, 511 - yy)) / 24.0 - 0.5, 0.0, 1.0)
    for q, fans in enumerate(ATLAS):
        tile = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
        for x, y, scale, deg, middle, width, shade in fans:
            off = np.abs((angle - middle + 180.0) % 360.0 - 180.0)
            keep = np.clip((width * 0.5 - off) / 12.0 + 0.5, 0.0, 1.0)
            keep = np.maximum(keep, np.clip((0.14 - radius) / 0.06, 0.0, 1.0))
            fan = card.copy()
            fan[..., 3] *= keep
            fan[..., :3] *= shade
            part = Image.fromarray(np.clip(fan * 255.0 + 0.5, 0, 255).astype(np.uint8), "RGBA")
            size = max(1, int(512 * scale))
            part = part.resize((size, size), Image.Resampling.LANCZOS)
            part = part.rotate(deg, resample=Image.Resampling.BICUBIC, expand=True)
            tile.alpha_composite(part, (int(x * 512 - part.width / 2), int(y * 512 - part.height / 2)))
        t = np.asarray(tile, dtype=np.float32) / 255.0
        t[..., 3] *= rim
        # Where nothing covers, the leaves' mean colour (the filtering then blends towards it, not to black).
        cover = t[..., 3] > 0.05
        fill = t[..., :3][cover].mean(axis=0) if cover.any() else np.array([0.5, 0.5, 0.5], dtype=np.float32)
        t[..., :3] = np.where(t[..., 3:] > 0.02, t[..., :3], fill)
        out.paste(Image.fromarray(np.clip(t * 255.0 + 0.5, 0, 255).astype(np.uint8), "RGBA"),
                  ((q % 2) * 512, (q // 2) * 512))
    return np.asarray(out, dtype=np.float32) / 255.0


def flat_maps(name, rough=0.85):
    """The maps a cut-out card needs beside its picture: an even ORM and a flat normal map."""
    save(np.full((4, 4, 3), [1.0, rough, 0.0], dtype=np.float32), name, "orm")
    save(np.full((4, 4, 3), [0.5, 0.5, 1.0], dtype=np.float32), name, "normal")


def save_rgba(a, path):
    Image.fromarray(np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8), "RGBA").save(path)


def blades():
    """Long blades of grass in a tuft, fanning out of one root, olive to straw (alpha: where a blade is): a cut-out card
    the gardens scatter over the lawn."""
    from PIL import ImageDraw
    rng = np.random.default_rng(11)
    size = 2048
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for _ in range(38):
        root = size / 2 + rng.uniform(-0.09, 0.09) * size
        length = rng.uniform(0.45, 0.95) * size
        lean = rng.uniform(-0.42, 0.42) * size
        width = rng.uniform(0.008, 0.016) * size
        tone = np.array([rng.uniform(0.33, 0.62), rng.uniform(0.42, 0.6), rng.uniform(0.12, 0.24)])
        left, right = [], []
        for i in range(17):
            t = i / 16
            x = root + lean * t * t
            y = size - 1 - length * t
            w = width * (1.0 - t) + 1.0
            left.append((x - w, y))
            right.append((x + w, y))
        shade = tuple(int(c * 255) for c in tone) + (255,)
        d.polygon(left + right[::-1], fill=shade)
    img = img.resize((512, 512), Image.Resampling.LANCZOS)
    save_rgba(np.asarray(img, dtype=np.float32) / 255.0, os.path.join(TEXTURES, "blades_albedo.png"))
    flat_maps("blades")


def leafcard(lum):
    """A cluster of leaves cut out of the foliage (its light leaves kept, the dark gaps between them clear), ragged at
    its rim: cards round the crowns, so that they end in leaves and not in a smooth skin."""
    h, w = lum.shape
    leaves = lum[h // 4:h // 4 + 512, w // 4:w // 4 + 512]
    yy, xx = np.mgrid[0:512, 0:512].astype(np.float32)
    r = np.sqrt((xx - 255.5) ** 2 + (yy - 255.5) ** 2) / 256.0
    rng = np.random.default_rng(5)
    wobble = blur(rng.standard_normal((512, 512, 1)).astype(np.float32), 18)[..., 0]
    wobble = wobble / max(float(np.abs(wobble).max()), 1e-3)
    inside = r + 0.25 * wobble < 0.82
    alpha = (inside & (leaves > np.percentile(leaves, 38))).astype(np.float32)
    rgb = np.repeat(np.clip(leaves * 1.1, 0, 1)[..., None], 3, axis=2)
    save_rgba(leaf_atlas(np.concatenate([rgb, alpha[..., None]], axis=2)), os.path.join(TEXTURES, "leafcard_albedo.png"))
    flat_maps("leafcard")


def needles():
    """A fir's branch seen from above, its tip at the top: a twig, side twigs, needles all along them, light grey so that
    a tint gives the colour (alpha: where it is). The firs are tiers of these round a trunk."""
    from PIL import ImageDraw
    rng = np.random.default_rng(17)
    size = 2048
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    def twig(x0, y0, x1, y1, width, depth):
        steps = int(np.hypot(x1 - x0, y1 - y0) / 9) + 1
        for i in range(steps):
            t = i / steps
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            length = (1.0 - 0.5 * t) * rng.uniform(95, 135) * (1.0 if depth == 0 else 0.75)
            ang = np.arctan2(y1 - y0, x1 - x0)
            for side in (-1, 1):
                a = ang + side * rng.uniform(0.7, 1.1)
                g = int(rng.uniform(150, 235))
                d.line([(x, y), (x + np.cos(a) * length, y + np.sin(a) * length)], fill=(g, g, g, 255), width=9)
        d.line([(x0, y0), (x1, y1)], fill=(110, 105, 100, 255), width=int(width))
        if depth == 0:
            for k in range(10):
                t = 0.08 + 0.085 * k
                bx, by = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
                for side in (-1, 1):
                    reach = (1.0 - 0.7 * t) * size * rng.uniform(0.38, 0.5)
                    ang = np.arctan2(y1 - y0, x1 - x0) + side * rng.uniform(0.45, 0.75)
                    twig(bx, by, bx + np.cos(ang) * reach, by + np.sin(ang) * reach, 9, 1)

    twig(size / 2, size - 40, size / 2 + rng.uniform(-40, 40), 60, 22, 0)
    img = img.resize((512, 512), Image.Resampling.LANCZOS)
    save_rgba(np.asarray(img, dtype=np.float32) / 255.0, os.path.join(TEXTURES, "needles_albedo.png"))
    flat_maps("needles")


def paths():
    """Patches of trodden earth with fine gravel, their rims fading out raggedly (alpha): decals laid one after another
    along the gardens' paths, so that the way runs soft into the grass."""
    dust = np.asarray(Image.open(os.path.join(TEXTURES, "dust_albedo.png")).convert("RGB").resize(
        (512, 512), Image.Resampling.LANCZOS), dtype=np.float32) / 255.0
    lum = dust.mean(axis=2)
    yy, xx = np.mgrid[0:512, 0:512].astype(np.float32)
    r = np.sqrt((xx - 255.5) ** 2 + (yy - 255.5) ** 2) / 256.0
    for n, seed in ((1, 21), (2, 22)):
        rng = np.random.default_rng(seed)
        wobble = blur(rng.standard_normal((512, 512, 1)).astype(np.float32), 22)[..., 0]
        wobble = wobble / max(float(np.abs(wobble).max()), 1e-3)
        grain = blur(rng.standard_normal((512, 512, 1)).astype(np.float32), 2)[..., 0]
        earth = lum[..., None] * np.array([0.62, 0.5, 0.36], dtype=np.float32) * (0.9 + 0.25 * grain[..., None])
        # Pebbles: small pale specks.
        pebbles = (rng.random((512, 512)) > 0.9965).astype(np.float32)[..., None]
        pebbles = np.clip(blur(pebbles, 1) * 6.0, 0, 1)
        earth = earth * (1 - pebbles) + np.array([0.62, 0.6, 0.55], dtype=np.float32) * pebbles
        alpha = np.clip((0.92 - (r + 0.2 * wobble)) / 0.3, 0, 1) ** 1.2
        save_rgba(np.concatenate([np.clip(earth, 0, 1), alpha[..., None]], axis=2),
                  os.path.join(TEXTURES, "decals", "path_%d.png" % n))


if __name__ == "__main__":
    if "--local" in sys.argv[1:]:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import make_textures
        print("painted locally: %d" % make_textures.paint_local(paint_jobs(), "garden"), flush=True)
    main()
