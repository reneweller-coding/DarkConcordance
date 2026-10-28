"""Builds the bodies of DARK CONCORDANCE's people from MPFB (MakeHuman for Blender; the assets CC0): body, skin, eyes,
brows, hair and clothes on MPFB's game engine rig, renamed as the game's clips want it ("root", "Head"), with an
"A_TPose" clip that stands the body in the animation library's T-pose (the game retargets every clip from it).
Written as GLB (the heroes' textures at most 1024 pixels, everyone else's 512) with a list beside it,
<name>.glb.txt, of what each of its meshes is, in the file's order: skin, eyes, cut (hair, brows, lashes: cut out
where their texture is clear), cloth (the game tints it per person) or armour (tinted per faction).

Runs in Blender (5.2, MPFB 2.0.18 installed with the asset packs):

    blender --background --factory-startup --python tools/make_heroes.py -- NAME [--clothes a,b] [--out GLB]
        [--preview PNG]

NAME is a hero (corvin, dorra, ione: assets/models/heroes) or one of the people (assets/models/people: the folk of
the hubs, the crowd's, the soldiers under the foes' helmets); --clothes replaces the clothes (to try others),
--preview renders a picture.
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
        "tint": {"jupiter7": (0.88, 0.88, 0.85), "guardian": (0.72, 0.72, 0.72)},
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
        "tint": {"helios": (1.0, 0.7, 0.3), "guardian": (0.75, 0.58, 0.36), "hair": (1.5, 0.72, 0.45)},
    },
    # Thin, a maintenance suit in grey-blue.
    "ione": {
        "phenotype": {"gender": 0.0, "age": 0.6, "muscle": 0.42, "weight": 0.3, "proportions": 0.7, "height": 0.5,
                      "cupsize": 0.4, "firmness": 0.5, "race": {"asian": 0.25, "caucasian": 0.7, "african": 0.05}},
        "skin": "onlytheghosts_young_eurasian_female/onlytheghosts_young_eurasian_female.mhmat",
        "hair": "bob02/bob02.mhclo",
        "eyebrows": "eyebrow009/eyebrow009.mhclo",
        "clothes": ["thegreatengineer_galactic_warrior_uniform", "matcreator_mc-scifi-armor_guardian"],
        "tint": {"galactic": (0.62, 0.72, 0.86), "guardian": (0.6, 0.66, 0.74), "hair": (0.35, 0.33, 0.33)},
    },
}


def person(gender, age, skin, hair, clothes, muscle=0.5, weight=0.5, height=0.5, race=(0.33, 0.33, 0.33), tint=None,
           brows="eyebrow003"):
    """One of the people: MakeHuman's macros, a skin, hair (none: "") and clothes (MakeHuman's names)."""
    return {"phenotype": {"gender": gender, "age": age, "muscle": muscle, "weight": weight, "proportions": 0.6,
                          "height": height, "cupsize": 0.5, "firmness": 0.5,
                          "race": {"african": race[0], "asian": race[1], "caucasian": race[2]}},
            "skin": "%s/%s.mhmat" % (skin, skin), "hair": "%s/%s.mhclo" % (hair, hair) if hair else "",
            "eyebrows": "%s/%s.mhclo" % (brows, brows), "clothes": clothes, "tint": tint or {}}


# The people with a portrait (tools/make_portraits.py, by its key; the game finds them by the look's name), the crowd
# of the hubs, and the men in the foes' suits (their helmets stay the game's, so no hair; the armour a light grey the
# faction's colour tints). Ages: 0.5 is 25 years, 0.69 about 50, 0.78 about 60, 0.12 about six.
PEOPLE = {
    "ysolde": person(0.0, 0.69, "middleage_caucasian_female", "rehmanpolanski_hair_bun_brown",
                     ["donitz_monk_robe", "donitz_monk_robe_hood_off", "shoes04"], height=0.8, weight=0.4, race=(0.05, 0.1, 0.85),
                     tint={"hair": (0.75, 0.7, 0.75), "monk": (0.5, 0.36, 0.62)}),
    "ottla": person(0.0, 0.78, "old_caucasian_female", "short01", ["male_worksuit01", "shoes03"], muscle=0.65,
                    weight=0.8, race=(0.1, 0.1, 0.8), tint={"hair": (1.3, 1.3, 1.3), "worksuit": (0.85, 0.68, 0.52)}),
    "kiri": person(0.0, 0.53, "young_african_female", "afro01", ["male_worksuit01", "shoes06"], muscle=0.6,
                   race=(0.6, 0.1, 0.3), tint={"hair": (0.3, 0.3, 0.3), "worksuit": (1.3, 0.62, 0.25)}),
    "okafor": person(0.0, 0.65, "middleage_african_female", "short01",
                     ["matcreator_mc-bodysuit-2021", "matcreator_mc-scifi-armor_helios", "shoes06"], muscle=0.75,
                     race=(0.9, 0.05, 0.05), tint={"hair": (1.1, 1.1, 1.1), "helios": (0.62, 0.66, 0.4)}),
    "pell": person(1.0, 0.95, "jartur69_old_slavic_male_with_genitals_and_beard", "cortu_short_messy_hair",
                   ["rehmanpolanski_viking_tunic", "rehmanpolanski_viking_pants", "rehmanpolanski_viking_boots"], weight=0.35, race=(0.05, 0.05, 0.9),
                   tint={"hair": (1.8, 1.8, 1.8), "viking": (0.8, 0.74, 0.62)}),
    "hale": person(0.0, 0.8, "old_caucasian_female", "short02", ["male_elegantsuit01", "shoes04"], weight=0.4,
                   race=(0.05, 0.05, 0.9), tint={"hair": (1.0, 1.0, 1.0), "elegantsuit": (0.5, 0.5, 0.56)}),
    "lamm": person(1.0, 0.55, "young_caucasian_male", "short04", ["culturalibre_hero_suit_1", "shoes06"], muscle=0.85,
                   height=0.75, race=(0.05, 0.05, 0.9), tint={"hair": (1.7, 1.35, 0.7), "hero_suit": (1.1, 0.4, 0.3)}),
    "ruul": person(1.0, 0.72, "middleage_caucasian_male", "", ["male_worksuit01", "shoes03"], weight=0.2,
                   race=(0.1, 0.1, 0.8), tint={"worksuit": (0.8, 0.75, 0.62)}),
    "ilka": person(0.0, 0.66, "middleage_caucasian_female", "short02", ["male_worksuit01", "shoes02"],
                   race=(0.1, 0.1, 0.8), tint={"hair": (0.4, 0.38, 0.38), "worksuit": (0.6, 0.6, 0.62)}),
    "ilkagirl": person(0.0, 0.16, "young_caucasian_female", "ponytail01", ["male_worksuit01", "shoes01"],
                       race=(0.1, 0.1, 0.8), tint={"hair": (0.45, 0.4, 0.38), "worksuit": (1.45, 1.2, 0.3)}),
    "mira": person(0.0, 0.13, "young_caucasian_female", "bob02", ["thegreatengineer_galactic_warrior_uniform", "shoes01"],
                   race=(0.05, 0.05, 0.9), tint={"hair": (0.95, 0.8, 0.55), "galactic": (1.25, 1.05, 0.3)}),
    "kind": person(1.0, 0.12, "toigo_light_skin_male_freckles", "short04", ["rehmanpolanski_viking_tunic", "rehmanpolanski_viking_pants", "rehmanpolanski_viking_boots"],
                   race=(0.05, 0.05, 0.9), tint={"viking": (0.85, 0.72, 0.55)}),
    "dreier": person(1.0, 0.72, "jartur69_middleage_slavic_male_with_genitals_and_beard", "",
                     ["matcreator_mc-bodysuit-2021", "matcreator_mc-scifi-armor_jupiter7", "shoes06"], muscle=0.9,
                     weight=0.75, height=0.72, race=(0.05, 0.05, 0.9), tint={"jupiter7": (0.5, 0.16, 0.15)}),
    "brask": person(1.0, 0.65, "middleage_caucasian_male", "short03", ["male_casualsuit05", "shoes03"], muscle=0.35,
                    weight=0.7, race=(0.1, 0.1, 0.8), tint={"casualsuit": (0.7, 0.7, 0.72)}),
    "siel": person(0.0, 0.64, "middleage_caucasian_female", "short01",
                   ["matcreator_mc-bodysuit-2021", "matcreator_mc-scifi-armor_helios", "shoes06"], muscle=0.6,
                   weight=0.35, race=(0.05, 0.1, 0.85), tint={"hair": (0.4, 0.38, 0.36), "helios": (0.6, 0.64, 0.4)}),
    "josse": person(1.0, 0.5, "young_caucasian_male", "short03", ["male_worksuit01", "shoes06"], muscle=0.85,
                    weight=0.6, race=(0.1, 0.1, 0.8), tint={"worksuit": (0.95, 0.72, 0.35)}),
    "worn1": person(1.0, 0.56, "middleage_caucasian_male", "short03", ["male_worksuit01", "shoes06"],
                    muscle=0.6, weight=0.55, race=(0.1, 0.1, 0.8)),
    "worn2": person(0.0, 0.5, "young_african_female", "braid01", ["rehmanpolanski_viking_tunic", "rehmanpolanski_viking_pants", "rehmanpolanski_viking_boots"],
                    race=(0.8, 0.1, 0.1)),
    "worn3": person(1.0, 0.82, "old_asian_male", "short01", ["male_casualsuit03", "shoes03"], weight=0.4,
                    race=(0.05, 0.85, 0.1), tint={"hair": (0.9, 0.9, 0.9)}),
    "crowd1": person(1.0, 0.45, "young_african_male", "short04", ["thegreatengineer_galactic_warrior_uniform",
                     "shoes05"], race=(0.85, 0.05, 0.1)),
    "crowd2": person(0.0, 0.47, "young_asian_female", "long01", ["culturalibre_heroine_suit_2", "shoes01"],
                     race=(0.05, 0.85, 0.1)),
    "crowd3": person(1.0, 0.62, "middleage_caucasian_male", "short02", ["male_casualsuit05", "shoes03"],
                     weight=0.65, race=(0.1, 0.1, 0.8)),
    "crowd4": person(0.0, 0.8, "old_caucasian_female", "bob02", ["donitz_monk_robe", "donitz_monk_robe_hood_off", "shoes02"],
                     race=(0.1, 0.1, 0.8), tint={"hair": (1.3, 1.3, 1.3)}),
    "crowd5": person(1.0, 0.42, "young_asian_male", "short03", ["male_worksuit01", "shoes06"],
                     race=(0.05, 0.85, 0.1)),
    "crowd6": person(0.0, 0.58, "middleage_african_female", "ponytail01", ["rehmanpolanski_viking_tunic", "rehmanpolanski_viking_pants", "rehmanpolanski_viking_boots"],
                     race=(0.85, 0.05, 0.1)),
    "soldier": person(1.0, 0.5, "middleage_caucasian_male", "", ["matcreator_mc-bodysuit-2021",
                      "matcreator_mc-scifi-armor_helios", "matcreator_mc-scifi-armor_guardian"], muscle=0.7,
                      race=(0.3, 0.3, 0.4)),
    "heavy": person(1.0, 0.55, "middleage_african_male", "", ["matcreator_mc-bodysuit-2021",
                    "matcreator_mc-scifi-armor_jupiter7", "matcreator_mc-scifi-armor_guardian"], muscle=0.95,
                    weight=0.8, race=(0.6, 0.1, 0.3)),
}
ALL = {**HEROES, **PEOPLE}
ARMOUR = ("scifi-armor", "galactic_warrior")   # clothes the faction's colour tints

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
    h = ALL[hero]
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
    # Two keys, from time 0 (raylib reads the first frame at time 0; before a clip's first key it keeps the rest pose).
    for frame in (0, 1):
        for pb in arm.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            pb.keyframe_insert("location", frame=frame)
    return act


def tint(meshes, tints, hair):
    """Multiplies the colour textures of the pieces whose names hold a key ("hair": the hair) by its colour."""
    import numpy as np
    done = set()
    for o in meshes:
        name = " ".join([o.name] + [s.material.name for s in o.material_slots if s.material]).lower()
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


def roles(meshes):
    """What each mesh is, by its data's name: skin, eyes, cut, cloth or armour."""
    from bl_ext.user_default.mpfb.services.objectservice import ObjectService
    out = {}
    for o in meshes:
        kind = ObjectService.get_object_type(o) or ""
        if kind == "Basemesh":
            role = "skin"
        elif kind == "Eyes":
            role = "eyes"
        elif kind in ("Eyebrows", "Eyelashes", "Hair"):
            role = "cut"
        else:
            # The material carries the asset's name (<person>.<asset>), the object and its mesh their own.
            text = " ".join((o.name, o.data.name, o.active_material.name if o.active_material else "")).lower()
            role = "armour" if any(w in text for w in ARMOUR) else "cloth"
        out[o.data.name] = role
    return out


def write_roles(glb, by_name):
    """Beside the GLB: one line per mesh the game loads (a glTF primitive), in the file's order."""
    import json
    import struct
    data = open(glb, "rb").read()
    n = struct.unpack("<I", data[12:16])[0]
    g = json.loads(data[20:20 + n])
    with open(glb + ".txt", "w", encoding="utf-8") as f:
        for m in g["meshes"]:
            for _ in m["primitives"]:
                f.write("%s %s\n" % (by_name.get(m["name"], "cloth"), m["name"]))


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
    ap.add_argument("hero", choices=sorted(ALL))
    ap.add_argument("--clothes", default="")
    ap.add_argument("--out", default="")
    ap.add_argument("--preview", default="")
    a = ap.parse_args(argv)
    clear()
    lib = library_tpose()
    clothes = [c for c in a.clothes.split(",") if c]
    h = ALL[a.hero]
    arm, meshes, basemesh = build(a.hero, clothes)
    kinds = roles(meshes)
    flatten(meshes)
    tpose(arm, lib)
    tint(meshes, h.get("tint", {}), h["hair"].split("/")[0] if h["hair"] else "-")
    shrink_textures(1024 if a.hero in HEROES else 512)
    if a.preview:
        preview(os.path.abspath(a.preview), arm)
    if a.out:
        os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
        bpy.ops.export_scene.gltf(filepath=a.out, export_format="GLB", export_apply=True, export_skins=True,
                                  export_animations=True, export_animation_mode="ACTIONS", export_yup=True,
                                  export_image_format="JPEG", export_jpeg_quality=88, export_morph=False,
                                  export_anim_slide_to_zero=True)
        write_roles(a.out, kinds)
    print("HERO", a.hero, "meshes", len(meshes), "bones", len(arm.data.bones), kinds)


main()
