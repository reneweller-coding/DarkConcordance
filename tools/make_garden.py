"""Derives the gardens' surfaces from textures there are (no image model, nothing asked for):

    G:\\Tools\\ImageGeneration\\venv\\Scripts\\python.exe tools\\make_garden.py
    py -3 tools/make_dds.py --only lawn lawn_v1 lawn_v2 foliage bark

  lawn      grass with most of its daisies gone (from the camera's height they flickered as white specks), a shade
            darker and calmer
  lawn_v1   grass_v1 the same way
  lawn_v2   the lawn turned and a little drier (grass_v2 has a path of stepping stones across it, which the patchwork
            laid everywhere as diagonal stripes)
  foliage   the hedge's leaves without their colour: the plants tint it (teal, violet, rust, ...)
  bark      dustrock stood upright (its layers run up a trunk) and greyed
  boulder   dustrock greyed and lightened: the stone of the gardens' rocky edges
  blades    a tuft of long grass blades (alpha: cut out), scattered over the lawn
  leafcard  a cluster of the foliage's leaves cut out with a ragged rim, round the crowns
  path_1/2  decals of trodden earth with gravel, their rims fading (assets/textures/decals)

Each writes <name>_albedo.png, _orm.png and _normal.png into assets/textures (blades and leafcard keep their alpha:
no DDS for them, DXT1 would cut it).
"""
import os

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEXTURES = os.path.join(ROOT, "assets", "textures")
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

    # Foliage: the hedge's luminance, brightened so that a tint gives the colour.
    leaf = load("hedge", "albedo")
    lum = leaf @ np.array([0.3, 0.55, 0.15], dtype=np.float32)
    lum = lum / max(float(lum.mean()), 1e-3) * 0.55
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
    leafcard(lum)
    paths()


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
    save_rgba(np.concatenate([rgb, alpha[..., None]], axis=2), os.path.join(TEXTURES, "leafcard_albedo.png"))
    flat_maps("leafcard")


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
    main()
