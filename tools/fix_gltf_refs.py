"""Takes out of glTF models' materials the textures whose files are not there (the Quaternius people ask for normal
and roughness maps they never came with; the game draws them with its own surfaces, but raylib looks for each one and
logs "Failed to open"). The images stay listed; only the materials stop asking for them.

    py -3 tools/fix_gltf_refs.py [FOLDER ...]      (default assets/models/quaternius)
"""
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def missing(folder, gltf, texture):
    """Whether a texture's image file is absent (an embedded or buffer image counts as there)."""
    textures = gltf.get("textures", [])
    if texture >= len(textures) or "source" not in textures[texture]:
        return False
    image = gltf.get("images", [])[textures[texture]["source"]]
    uri = image.get("uri")
    return bool(uri) and not uri.startswith("data:") and not os.path.exists(os.path.join(folder, uri))


def fix(path):
    folder = os.path.dirname(path)
    gltf = json.load(open(path, encoding="utf-8"))
    dropped = 0
    for material in gltf.get("materials", []):
        for key in ("normalTexture", "occlusionTexture", "emissiveTexture"):
            if key in material and missing(folder, gltf, material[key]["index"]):
                del material[key]
                dropped += 1
        pbr = material.get("pbrMetallicRoughness", {})
        for key in ("baseColorTexture", "metallicRoughnessTexture"):
            if key in pbr and missing(folder, gltf, pbr[key]["index"]):
                del pbr[key]
                dropped += 1
    if dropped:
        part = path + ".part"
        with open(part, "w", encoding="utf-8") as f:
            json.dump(gltf, f, indent=1)
        os.replace(part, path)
        print("%s: %d references to missing textures taken out" % (os.path.basename(path), dropped))


def main():
    folders = sys.argv[1:] or [os.path.join(ROOT, "assets", "models", "quaternius")]
    for folder in folders:
        for path in sorted(glob.glob(os.path.join(folder, "*.gltf"))):
            fix(path)


if __name__ == "__main__":
    main()
