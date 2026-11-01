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

Each writes <name>_albedo.png, _orm.png and _normal.png into assets/textures.
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
    maps["albedo"] = (grey + (a - grey) * 0.85) * 0.78
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


if __name__ == "__main__":
    main()
