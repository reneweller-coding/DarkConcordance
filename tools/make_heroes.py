"""Builds the heroes' bodies for DARK CONCORDANCE from MPFB (MakeHuman for Blender; the assets CC0): body, skin, eyes,
brows, hair and suit on MPFB's game engine rig, renamed as the game's clips want it ("root", "Head"), with an
"A_TPose" clip that stands the body in the animation library's T-pose (the game retargets every clip from it).
Written as GLB with the textures at most 1024 pixels.

Runs in Blender (5.2, MPFB 2.0.18 installed with the asset packs):

    blender --background --factory-startup --python tools/make_heroes.py -- HERO [--clothes a,b] [--out GLB]
        [--preview PNG]

HERO is corvin, dorra or ione; --clothes replaces the suit (to try others), --preview renders a picture.
"""
import argparse
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TPOSE = os.path.join(ROOT, "assets", "models", "quaternius", "anim_library2.glb")

# The heroes as their portraits (tools/make_portraits.py): MakeHuman's macros (age 0.5 is 25 years, 1.0 is 90).
HEROES = {
    # A worn grey hardsuit: plates over a black undersuit.
    "corvin": {
        "phenotype": {"gender": 1.0, "age": 0.68, "muscle": 0.75, "weight": 0.5, "proportions": 0.85, "height": 0.62,
                      "cupsize": 0.5, "firmness": 0.5, "race": {"asian": 0.1, "caucasian": 0.8, "african": 0.1}},
        "skin": "jartur69_middleage_slavic_male_with_genitals_and_beard/"
                "jartur69_middleage_slavic_male_with_genitals_and_beard.mhmat",
        "hair": "short02/short02.mhclo",
        "eyebrows": "eyebrow001/eyebrow001.mhclo",
        "clothes": ["matcreator_mc-bodysuit-2021", "matcreator_mc-scifi-armor_jupiter7",
                    "matcreator_mc-scifi-armor_guardian"],
        "tint": {"jupiter7": (0.62, 0.62, 0.6), "guardian": (0.5, 0.5, 0.5)},
    },
    # Short and very broad from a high-gravity world; dented ochre plates of an old mining suit; copper-red hair.
    "dorra": {
        "phenotype": {"gender": 0.0, "age": 0.66, "muscle": 0.9, "weight": 0.8, "proportions": 0.5, "height": 0.08,
                      "cupsize": 0.45, "firmness": 0.6, "race": {"asian": 0.05, "caucasian": 0.9, "african": 0.05}},
        "skin": "toigo_light_skin_female_freckles/toigo_light_skin_female_freckles.mhmat",
        "hair": "rehmanpolanski_hair_bun_brown/rehmanpolanski_hair_bun_brown.mhclo",
        "eyebrows": "eyebrow002/eyebrow002.mhclo",
        "clothes": ["matcreator_mc-bodysuit-2021", "matcreator_mc-scifi-armor_helios",
                    "matcreator_mc-scifi-armor_guardian"],
        "tint": {"helios": (0.85, 0.58, 0.22), "guardian": (0.55, 0.42, 0.25), "bodysuit": (1.0, 0.8, 0.6),
                 "hair": (1.5, 0.72, 0.45)},
    },
    # Thin, a maintenance suit in grey-blue.
    "ione": {
        "phenotype": {"gender": 0.0, "age": 0.6, "muscle": 0.42, "weight": 0.3, "proportions": 0.7, "height": 0.5,
                      "cupsize": 0.4, "firmness": 0.5, "race": {"asian": 0.25, "caucasian": 0.7, "african": 0.05}},
        "skin": "onlytheghosts_young_eurasian_female/onlytheghosts_young_eurasian_female.mhmat",
        "hair": "bob02/bob02.mhclo",
        "eyebrows": "eyebrow009/eyebrow009.mhclo",
        "clothes": ["thegreatengineer_galactic_warrior_uniform", "matcreator_mc-scifi-armor_guardian"],
        "tint": {"galactic": (0.5, 0.6, 0.72), "guardian": (0.45, 0.5, 0.56), "hair": (0.35, 0.33, 0.33)},
    },
}

# Each bone of the rig and the joint it points at, as in the animation library (a leaf: the library's end joint,
# the rig's own tail).
POINTS = {"pelvis": "spine_01", "spine_01": "spine_02", "spine_02": "spine_03", "spine_03": "neck_01",
          "neck_01": "Head"}
for s in "lr":
    POINTS.update({"clavicle_" + s: "upperarm_" + s, "upperarm_" + s: "lowerarm_" + s, "lowerarm_" + s: "hand_" + s,
                   "hand_" + s: "middle_01_" + s, "thigh_" + s: "calf_" + s, "calf_" + s: "foot_" + s,
                   "foot_" + s: "ball_" + s, "ball_" + s: "ball_leaf_" + s})
    for f in ("index", "middle", "ring", "pinky", "thumb"):
        POINTS.update({"%s_01_%s" % (f, s): "%s_02_%s" % (f, s), "%s_02_%s" % (f, s): "%s_03_%s" % (f, s),
                       "%s_03_%s" % (f, s): "%s_04_leaf_%s" % (f, s)})


def mpfb():
    """The MPFB services (the extension's package)."""
    bpy.ops.preferences.addon_enable(module="bl_ext.user_default.mpfb")
    from bl_ext.user_default.mpfb.services.humanservice import HumanService
    return HumanService


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mpfb()


def build(hero, clothes):
    """The human with rig, skin, eyes, brows, hair and clothes; returns (armature, meshes)."""
    HumanService = mpfb()
    h = HEROES[hero]
    info = {
        "name": hero, "phenotype": h["phenotype"], "rig": "game_engine",
        "eyes": "high-poly/high-poly.mhclo", "eyebrows": h["eyebrows"], "eyelashes": "eyelashes01/eyelashes01.mhclo",
        "tongue": "", "teeth": "", "hair": h["hair"], "proxy": "", "targets": [],
        "clothes": [c if c.endswith(".mhclo") else "%s/%s.mhclo" % (c, c) for c in clothes or h["clothes"]],
        "skin_mhmat": h["skin"], "skin_material_type": "MAKESKIN", "eyes_material_type": "MAKESKIN",
        "clothes_material_type": "MAKESKIN", "skin_material_settings": {}, "eyes_material_settings": {},
        "expressions": [], "alternative_materials": {},
    }
    settings = HumanService.get_default_deserialization_settings()
    settings["subdiv_levels"] = 0
    basemesh = HumanService.deserialize_from_dict(info, settings)
    arm = basemesh.parent
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    return arm, meshes, basemesh


def flatten(meshes):
    """Shape keys baked in, the masks (the body hidden under the clothes) applied: what the game draws."""
    for o in meshes:
        bpy.context.view_layer.objects.active = o
        for s in bpy.context.selected_objects:
            s.select_set(False)
        o.select_set(True)
        if o.data.shape_keys:
            bpy.ops.object.shape_key_remove(all=True, apply_mix=True)
        for m in list(o.modifiers):
            if m.type in ("MASK", "SUBSURF"):
                bpy.ops.object.modifier_apply(modifier=m.name)


def joints(arm):
    """Each pose bone's head in world space (and the leaves' tails under their library names)."""
    out = {}
    for pb in arm.pose.bones:
        out[pb.name] = arm.matrix_world @ pb.head
    return out


def basis(j):
    """The rig's frame from hips, head and thighs (as the game measures it)."""
    up = (j["Head"] - j["pelvis"]).normalized()
    side = (j["thigh_l"] - j["thigh_r"]).normalized()
    fwd = side.cross(up).normalized()
    side = up.cross(fwd).normalized()
    return Matrix((side, up, fwd)).transposed()


def library_tpose():
    """The animation library's T-pose: its joints in world space, imported and removed again."""
    before = set(bpy.data.objects)
    actions_before = set(bpy.data.actions)
    bpy.ops.import_scene.gltf(filepath=TPOSE)
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    act = next(a for a in bpy.data.actions if a not in actions_before and a.name.startswith("A_TPose"))
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and act.slots:
        arm.animation_data.action_slot = act.slots[0]
    bpy.context.scene.frame_set(int(act.frame_range[0]))
    bpy.context.view_layer.update()
    j = joints(arm)
    for o in new:
        bpy.data.objects.remove(o, do_unlink=True)
    for a in list(bpy.data.actions):
        if a not in actions_before:
            bpy.data.actions.remove(a)
    return j


def tpose(arm, lib):
    """Turns each bone (parents first) to point where the library's does, keyed as the clip "A_TPose"."""
    for b in arm.data.bones:
        if b.name == "Root":
            b.name = "root"
        elif b.name == "head":
            b.name = "Head"
    bpy.context.view_layer.update()
    turn = basis(joints(arm)) @ basis(lib).inverted()
    inv = arm.matrix_world.to_3x3().inverted()
    order = []

    def walk(b):
        order.append(b.name)
        for c in b.children:
            walk(c)

    for b in arm.data.bones:
        if b.parent is None:
            walk(b)
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    for name in order:
        if name not in POINTS or name not in lib or POINTS[name] not in lib:
            continue
        pb = arm.pose.bones[name]
        j = joints(arm)
        tip = j[POINTS[name]] if POINTS[name] in j else arm.matrix_world @ pb.tail
        have = inv @ (tip - j[name])
        want = inv @ (turn @ (lib[POINTS[name]] - lib[name]))
        if have.length < 1e-5 or want.length < 1e-5:
            continue
        q = have.normalized().rotation_difference(want.normalized())
        head = pb.head.copy()
        pb.matrix = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-head) @ pb.matrix
        bpy.context.view_layer.update()
    arm.animation_data_create()
    act = bpy.data.actions.new("A_TPose")
    arm.animation_data.action = act
    for pb in arm.pose.bones:
        pb.keyframe_insert("rotation_quaternion", frame=1)
        pb.keyframe_insert("location", frame=1)
    return act


def tint(meshes, tints, hair):
    """Multiplies the colour textures of the pieces whose names hold a key ("hair": the hair) by its colour."""
    import numpy as np
    done = set()
    for o in meshes:
        name = o.name.lower()
        for key, rgb in tints.items():
            if (hair.lower() if key == "hair" else key) not in name:
                continue
            for slot in o.material_slots:
                m = slot.material
                if not m or not m.node_tree:
                    continue
                for n in m.node_tree.nodes:
                    img = n.image if n.type == "TEX_IMAGE" else None
                    if not img or img.name in done or img.colorspace_settings.name != "sRGB":
                        continue
                    px = np.empty(len(img.pixels), dtype=np.float32)
                    img.pixels.foreach_get(px)
                    px = px.reshape(-1, 4)
                    px[:, :3] = np.clip(px[:, :3] * np.array(rgb, dtype=np.float32), 0.0, 1.0)
                    img.pixels.foreach_set(px.ravel())
                    img.update()
                    done.add(img.name)


def shrink_textures(limit=1024):
    """At most limit pixels a side; every image packed so the export writes what is in memory."""
    for img in bpy.data.images:
        if img.size[0] > limit or img.size[1] > limit:
            f = limit / max(img.size[0], img.size[1])
            img.scale(max(1, int(img.size[0] * f)), max(1, int(img.size[1] * f)))
        if img.size[0] > 0 and img.has_data:
            img.pack()


def preview(path, arm):
    """A picture of the hero in the T-pose: three-quarter view, a key light and a rim."""
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_EEVEE"
    scn.render.resolution_x, scn.render.resolution_y = 900, 1200
    scn.render.film_transparent = False
    world = bpy.data.worlds.new("w")
    world.color = (0.05, 0.05, 0.06)
    scn.world = world
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scn.collection.objects.link(cam)
    cam.location = (2.6, -4.2, 1.35)
    cam.rotation_euler = (math.radians(86), 0, math.radians(32))
    cam.data.lens = 50
    scn.camera = cam
    for name, loc, energy in (("key", (3, -3, 4), 900), ("rim", (-3, 3, 3), 600), ("fill", (-3, -4, 1), 200)):
        light = bpy.data.objects.new(name, bpy.data.lights.new(name, "AREA"))
        light.data.energy = energy
        light.data.size = 3
        light.location = loc
        light.rotation_euler = (Vector((0, 0, 1)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        scn.collection.objects.link(light)
    scn.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("hero", choices=sorted(HEROES))
    ap.add_argument("--clothes", default="")
    ap.add_argument("--out", default="")
    ap.add_argument("--preview", default="")
    a = ap.parse_args(argv)
    clear()
    lib = library_tpose()
    clothes = [c for c in a.clothes.split(",") if c]
    arm, meshes, basemesh = build(a.hero, clothes)
    flatten(meshes)
    tpose(arm, lib)
    tint(meshes, HEROES[a.hero].get("tint", {}), HEROES[a.hero]["hair"].split("/")[0])
    shrink_textures()
    if a.preview:
        preview(os.path.abspath(a.preview), arm)
    if a.out:
        os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
        bpy.ops.export_scene.gltf(filepath=a.out, export_format="GLB", export_apply=True, export_skins=True,
                                  export_animations=True, export_animation_mode="ACTIONS", export_yup=True,
                                  export_image_format="JPEG", export_jpeg_quality=88, export_morph=False)
    print("HERO", a.hero, "meshes", len(meshes), "bones", len(arm.data.bones))


main()
