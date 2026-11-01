"""Dresses the places: variants of every wall and floor texture (the levels lay them as a patchwork) and decals (stains,
cracks, posters, graffiti, neon, frost, glyphs), all painted by Gemini's image model (the same access as
make_textures.py).

    G:\\Tools\\ImageGeneration\\venv\\Scripts\\python.exe tools\\make_dress.py [--force NAME ...] [--only NAME ...]
        [--part k/n] [--list]

Variants: assets/textures/<base>_v<n>_{albedo,normal,orm}.png, their colours pulled towards the plain one's.
Decals:   assets/textures/decals/<name>.png (RGBA, the background keyed out) and assets/textures/decals/decals.txt,
          which the game reads: per decal where it goes, its size, roughness, glow and the themes that use it.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert( 0, os.path.dirname( os.path.abspath( __file__ ) ) )
import make_textures  # noqa: E402  (generate(), tile_maps(), the paths, the prompts of the plain textures)

make_textures.MODELS = ["gemini-3.1-flash-image", "gemini-3-pro-image"]
make_textures.LOCATIONS = ["global"]
make_textures.IMAGE_SIZE = "1K"
OUT = make_textures.OUT
DECALS = os.path.join( OUT, "decals" )
RAWDIR = os.path.join( make_textures.RAW, "dress" )

# ---------------------------------------------------------------------------------------------------------- variants
# Two more of each plain texture: the same material with something of its own in it.
VARIANTS = {
    "rock": ["with a vein of pale quartz and fresh pick marks", "with dark damp seepage and small white mineral crusts"],
    "floor": ["with a square drain grate and bolted cable conduit covers set into the plates",
              "with welded repair patches, scorch marks and a faded painted stripe"],
    "wall": ["with a large access hatch with recessed handles and worn warning paint",
             "with a cluster of pipes entering the wall, a small control box and strapped cable bundles"],
    "plaster": ["with plaster peeling off and bricks showing underneath, water stains",
                "with patched repairs in a lighter plaster, old nail holes and a crack"],
    "streetfloor": ["with a round maintenance hatch and worn painted lane lines",
                    "with cracked slabs, weeds in the joints and old dark stains"],
    "hullplate": ["with a recessed hatch, heavy bolts and stencilled hazard chevrons",
                  "with micrometeorite pitting and blue-brown heat discolouration"],
    "vaultwall": ["with a recessed panel of small locked drawers", "with an engraved decorative band and brass trim"],
    "vaultfloor": ["with an inlaid brass line pattern", "with darker polished stone squares and a border band"],
    "frost": ["with thick hoarfrost crystals and frozen drips", "with the frost partly melted showing panels beneath"],
    "iceblock": ["with deep blue cracks and trapped air bubbles", "with a dusting of snow powder and scratches"],
    "snowfield": ["with footprints and wind-drifted ridges", "with a patch of exposed dark ice and wind-swept lines"],
    "deckrust": ["with a hatch and heavy rust bloom", "with a newer replacement plate bolted in, less rusted"],
    "dustrock": ["with wind-carved layers", "with cracks filled with sand"],
    "dust": ["with tyre tracks and footprints", "with scattered pebbles and a dry crack pattern"],
    "podwall": ["with a recessed status panel and a soft blue light strip",
                "with scuffs, a dented panel and taped repairs"],
    "tiles": ["with a few cracked and missing tiles", "with a stripe of coloured guidance tiles"],
    "scorched": ["with deep burn craters and molten drips", "with soot and ash drifts over the metal"],
    "obsidian": ["with faint glowing veins of cyan light", "with carved geometric alien grooves"],
    "hedge": ["with small white flowers", "with a thinner gap showing branches and dry leaves"],
    "grass": ["with clover patches and small wildflowers", "with a worn dirt path and stepping stones"],
    "sandstone": ["with carved ancient reliefs", "with erosion holes and a long crack"],
    "sand": ["with ripples from the wind", "with scattered pebbles and a half-buried bone"],
    "yellowplate": ["with heavy scratches and black tyre marks", "with a hazard stripe band and bolts"],
    "marble": ["with a brass inlay line pattern", "with darker veined marble slabs"],
    "slag": ["with glowing orange cracks", "with cooled glassy black pools"],
    "serverwall": ["with a rack of status lights and cable patch panels", "with an open panel showing circuit boards"],
    "serverfloor": ["with a perforated vent tile", "with cable trays visible under a missing tile"],
    "netwall": ["with brighter grid nodes", "with a glitching, corrupted section"],
    "netgrid": ["with brighter grid nodes", "with a glitching, corrupted section"],
    "regolith": ["with small craters and boot prints", "with scattered dark rocks"],
    "brick": ["with a bricked-up arch", "with soot stains and missing bricks"],
    "wetcobble": ["with a puddle and a drain", "with moss in the joints"],
    "toolwall": ["with different tools hanging on hooks", "with a shelf of jars and unlabelled boxes"],
    "ochrerock": ["with dark iron streaks", "with crumbling layers"],
    "minefloor": ["with the rails of a mine cart track", "with spilled ore pebbles"],
    "farmsoil": ["with rows of seedlings", "with an irrigation pipe and puddles"],
    "algae": ["with bubbles and a darker patch", "with tangled strands"],
    "wetfloor": ["with a drain grate and a film of running water", "with spots of algae slime"],
}

# ------------------------------------------------------------------------------------------------------------ decals
# Which themes a group of decals dresses (the names of Level.cpp's themes).
GROUPS = {
    "industrial": "service exchanger shaft works cargo gangway deck station hold yard furnace container prison hull",
    "street": "street bar",
    "clean": "bridge vault pods clinic server hall cryo net",
    "cold": "coldstore ice snow cryo",
    "outdoor": "dusk arena moon snow",
    "alien": "alien",
    "garden": "park",
}
TOP = ( "Top-down orthographic view of {}, isolated in the centre, nothing else, on a plain pure {} background, flat "
        "even lighting, photorealistic, no text, no letters, no logos, no frame" )
FRONT = ( "Straight front view of {}, filling the frame, flat even lighting, highly detailed, no real letters or words "
          "(invented alien-looking glyphs only where writing would be), no logos, no frame" )
ISOLATED = ( "Straight front view of {}, isolated in the centre, nothing else, on a plain pure {} background, flat even "
             "lighting, highly detailed, no text, no letters, no logos, no frame" )


def decal( name, where, groups, key, size, prompt, rough=0.8, glow=0.0, y=1.6, aspect="1:1", mul=None ):
    """One decal: key white (dark things), black (bright or glowing things) or none (an opaque plate or poster).
    mul: laid by multiplying (a stain darkens what is under it, its grain shows through); by default the stains."""
    if mul is None:
        mul = key == "white" and name.split( "_" )[0] in MULTIPLY
    return dict( name=name, where=where, groups=groups.split(), key=key, size=size, prompt=prompt, rough=rough,
                 glow=glow, y=y, aspect=aspect, mul=mul )


# Stains, cracks, soot, prints: multiplied over the floor or the wall.
MULTIPLY = { "oil", "ruststain", "crack", "scorch", "boots", "blood", "scuff", "snowprints", "tracks", "crater",
             "ruststreak", "waterstain", "soot", "impacts", "handprints", "spill", "mud" }


def floor( name, groups, key, size, what, **kw ):
    return decal( name, "floor", groups, key, size, TOP.format( what, key ), **kw )


def wall( name, groups, key, size, what, **kw ):
    template = FRONT if key == "none" else ISOLATED
    return decal( name, "wall", groups, key, size, template.format( what, key ) if key != "none" else
                  template.format( what ), **kw )


DECAL_LIST = [
    # Floors of the works, holds and ships.
    floor( "oil_1", "industrial street", "white", 1.6, "a dark glossy oil stain with splatter", rough=0.25 ),
    floor( "oil_2", "industrial", "white", 1.4, "a long smeared trail of black engine oil with drops", rough=0.25 ),
    floor( "oil_3", "industrial", "white", 1.2, "a small pool of brown hydraulic fluid with drips", rough=0.25 ),
    floor( "ruststain_1", "industrial", "white", 1.2, "an orange-brown rust ring left by a barrel" ),
    floor( "ruststain_2", "industrial", "white", 1.6, "a large blotchy orange rust stain spreading over metal" ),
    floor( "coolant_1", "industrial", "white", 1.6, "a puddle of blue-green coolant with an oily sheen", rough=0.08 ),
    floor( "coolant_2", "industrial cold", "white", 1.3, "a shallow puddle of dirty water with ripples", rough=0.05 ),
    floor( "crack_1", "industrial outdoor", "white", 1.8, "only thin dark jagged crack lines, a long fracture with a few branches, drawn alone with no floor, plate or material around them" ),
    floor( "crack_2", "industrial street clean", "white", 1.4, "only thin dark spider-web crack lines radiating from a point, drawn alone with no floor, concrete or material around them" ),
    floor( "scorch_1", "industrial outdoor", "white", 1.8, "a black scorch mark from an explosion, radial soot" ),
    floor( "scorch_2", "industrial", "white", 1.3, "a burnt patch with ash and melted droplets" ),
    floor( "debris_1", "industrial", "white", 1.4, "scattered bolts, nuts, washers and metal shavings" ),
    floor( "debris_2", "industrial", "white", 1.5, "broken pieces of a robot: bent plates, wires, a cracked lens" ),
    floor( "debris_3", "industrial street", "white", 1.4, "shards of broken glass and a dented tin can" ),
    floor( "marking_1", "industrial", "black", 1.8, "faded worn painted yellow floor chevron arrows" ),
    floor( "marking_2", "industrial clean", "black", 1.8, "a worn painted white landing circle with a cross" ),
    floor( "marking_3", "industrial", "black", 1.6, "worn painted red and white keep-clear hatching" ),
    floor( "boots_1", "industrial street", "white", 1.6, "a trail of dirty boot prints" ),
    floor( "boots_2", "outdoor", "white", 1.6, "boot prints and drag marks in fine dust" ),
    floor( "cables_1", "industrial", "white", 1.6, "a tangle of thick black and orange power cables lying flat" ),
    floor( "cables_2", "industrial clean", "white", 1.6, "a coiled grey hose with a brass coupling" ),
    floor( "papers_1", "industrial street clean", "white", 1.3, "scattered crumpled papers, cards and wrappers" ),
    floor( "papers_2", "industrial", "white", 1.2, "a dropped clipboard, loose data cards and a broken pen" ),
    floor( "blood_1", "industrial street outdoor", "white", 1.4, "a dried dark brown blood smear with drops" ),
    floor( "blood_2", "industrial", "white", 1.2, "a dried pool of dark blood with a hand print" ),
    floor( "drain_1", "industrial street", "white", 1.0, "a round cast-iron drain grate with a geometric pattern" ),
    floor( "drain_2", "industrial", "white", 1.1, "a square steel floor hatch with a recessed handle and bolts" ),
    # Streets and bars.
    floor( "litter_1", "street", "white", 1.4, "street litter: food wrappers, cans, a crushed paper cup" ),
    floor( "litter_2", "street", "white", 1.4, "spilled noodles, chopsticks and a takeaway box" ),
    floor( "litter_3", "street garden", "white", 1.3, "cigarette butts, bottle caps and a torn ticket" ),
    floor( "chalk_1", "street", "black", 1.6, "a child's chalk drawing of stars and a rocket, pastel lines" ),
    floor( "chalk_2", "street", "black", 1.6, "a chalk hopscotch grid with invented number glyphs" ),
    floor( "spill_1", "street clean", "white", 1.1, "a spilled sticky drink with drops", rough=0.12, mul=False ),
    floor( "spill_2", "street", "white", 1.0, "old dark gum spots and a coffee ring" ),
    floor( "manhole_1", "street", "white", 1.2, "a round cast-iron manhole cover with an embossed star pattern" ),
    floor( "manhole_2", "street", "white", 1.1, "a hexagonal utility hatch cover with worn embossed glyphs" ),
    # Clean halls.
    floor( "guide_1", "clean", "black", 1.9, "painted floor guidance lines in teal and orange curving" ),
    floor( "guide_2", "clean", "black", 1.6, "a painted floor arrow in pale blue, slightly worn" ),
    floor( "guide_3", "clean", "black", 1.6, "a painted floor circle of dots in soft green" ),
    floor( "scuff_1", "clean", "white", 1.6, "black rubber scuff marks from trolley wheels" ),
    floor( "scuff_2", "clean industrial", "white", 1.4, "grey dirty footprints and wheel tracks" ),
    floor( "spill_3", "clean", "white", 1.0, "a small spill of clear liquid with drops", rough=0.05, mul=False ),
    floor( "medwaste_1", "clean", "white", 1.1, "a dropped used syringe, torn gauze and a rubber glove" ),
    floor( "emblem_1", "clean", "white", 1.8, "an inlaid round emblem of an abstract eight-pointed star in brass" ),
    floor( "emblem_2", "clean", "white", 1.8, "an inlaid round emblem of a planet with an orbit ring in brass" ),
    # Cold places.
    floor( "frost_1", "cold", "black", 1.8, "a patch of white feathery hoarfrost crystals" ),
    floor( "frost_2", "cold", "black", 1.5, "a thin crust of frost with a few bare spots" ),
    floor( "frost_3", "cold industrial", "black", 1.2, "a frozen puddle with white air bubbles", rough=0.1 ),
    floor( "snowdrift_1", "cold", "black", 1.8, "a soft drift of white powder snow" ),
    floor( "snowdrift_2", "cold", "black", 1.6, "a scattered dusting of snow" ),
    floor( "icecrack_1", "cold", "black", 1.7, "only thin white fracture lines like cracks in ice, drawn alone with no ice block or material around them" ),
    floor( "icecrack_2", "cold", "black", 1.5, "only a star of thin pale blue-white crack lines, drawn alone with no ice or material around them" ),
    floor( "snowprints_1", "cold outdoor", "white", 1.6, "footprints pressed in white snow, soft grey shadows" ),
    floor( "snowprints_2", "cold", "white", 1.6, "tracks of a sledge in snow" ),
    # Outdoors: dust, sand, rocks.
    floor( "sanddrift_1", "outdoor", "black", 1.8, "a drift of fine ochre sand" ),
    floor( "sanddrift_2", "outdoor", "black", 1.6, "wind-blown streaks of pale sand" ),
    floor( "sanddrift_3", "outdoor industrial", "black", 1.4, "a small heap of grey dust" ),
    floor( "pebbles_1", "outdoor", "white", 1.4, "scattered small rocks and pebbles" ),
    floor( "pebbles_2", "outdoor", "white", 1.6, "broken flat rock slabs with gravel" ),
    floor( "tracks_1", "outdoor", "white", 1.8, "only two parallel dark tyre track lines with a tread pattern, drawn alone with no ground or material around them" ),
    floor( "tracks_2", "outdoor cold", "white", 1.6, "the tracks of a six-legged walking machine" ),
    floor( "bones_1", "outdoor", "black", 1.5, "a scattered bleached skeleton of a small alien animal" ),
    floor( "crater_1", "outdoor", "white", 1.8, "a small impact crater with ejecta rays" ),
    # The object of Tesh.
    floor( "glyphfloor_1", "alien", "black", 1.9, "a glowing cyan alien glyph circle of thin luminous lines",
           glow=2.5 ),
    floor( "glyphfloor_2", "alien", "black", 1.7, "a glowing violet alien spiral of luminous dots", glow=2.5 ),
    floor( "glyphfloor_3", "alien", "black", 1.8, "faint glowing white geometric alien lines", glow=1.6 ),
    floor( "shards_1", "alien", "black", 1.2, "scattered glowing violet crystal shards", glow=1.2 ),
    floor( "shards_2", "alien outdoor", "black", 1.2, "scattered pale blue crystal shards", glow=0.8 ),
    # Gardens.
    floor( "leaves_1", "garden", "white", 1.5, "fallen red and gold autumn leaves" ),
    floor( "leaves_2", "garden", "white", 1.4, "fallen green leaves and twigs" ),
    floor( "leaves_3", "garden street", "white", 1.2, "fallen pale pink blossom petals" ),
    floor( "flowers_1", "garden", "white", 1.4, "a patch of small blue and white wildflowers" ),
    floor( "flowers_2", "garden", "white", 1.4, "a patch of small yellow flowers and clover" ),
    # (No mud: on the gardens' clipped lawn its dark patches looked like holes.)

    # Walls of the works, holds and ships.
    wall( "ruststreak_1", "industrial", "white", 1.4, "vertical rust streaks running down from a row of bolts",
          y=2.4, aspect="3:4" ),
    wall( "ruststreak_2", "industrial", "white", 1.6, "long orange rust drips running down", y=2.2, aspect="3:4" ),
    wall( "ruststreak_3", "industrial outdoor", "white", 1.2, "a rust bloom around a leaking pipe flange",
          y=2.0 ),
    wall( "waterstain_1", "industrial street", "white", 1.6, "a dark water stain with drip trails", y=2.2,
          aspect="3:4" ),
    wall( "waterstain_2", "industrial clean", "white", 1.4, "a pale limescale stain with drips", y=2.2,
          aspect="3:4" ),
    wall( "soot_1", "industrial", "white", 1.6, "a black soot scorch mark rising upward", y=1.6, aspect="3:4" ),
    wall( "soot_2", "industrial outdoor", "white", 1.4, "a burnt blast mark with radial soot", y=1.4 ),
    wall( "impacts_1", "industrial", "white", 1.2, "bullet impact holes with rings of cracked paint", y=1.4 ),
    wall( "impacts_2", "industrial street", "white", 1.4, "only three deep parallel dark claw scratch lines, drawn alone with no panel, plate or material around them",
          y=1.3 ),
    wall( "poster_work_1", "industrial", "none", 1.25, "a faded propaganda poster of a heroic miner with a drill "
          "under a red sun, bold graphic print, torn edges and tape", y=1.7, aspect="3:4" ),
    wall( "poster_work_2", "industrial", "none", 1.25, "a faded safety poster: a worker in a helmet pointing at the "
          "viewer, bold graphic print, peeling corners", y=1.7, aspect="3:4" ),
    wall( "poster_work_3", "industrial", "none", 1.25, "a faded corporate poster of a giant ore freighter over a "
          "planet, retro print, water damaged", y=1.7, aspect="3:4" ),
    wall( "poster_work_4", "industrial street", "none", 1.25, "a faded poster for a mining colony: a smiling "
          "family under a glass dome, retro print, scratched, no lettering at all", y=1.7, aspect="3:4" ),
    wall( "safety_1", "industrial", "none", 0.6, "a worn square safety pictogram plate: a figure falling into a "
          "shaft, black on yellow", y=1.8 ),
    wall( "safety_2", "industrial", "none", 0.6, "a worn square safety pictogram plate: a radiation trefoil, black "
          "on yellow", y=1.8 ),
    wall( "safety_3", "industrial", "none", 0.6, "a worn square safety pictogram plate: a lightning bolt, white on "
          "red", y=1.8 ),
    wall( "safety_4", "industrial cold", "none", 0.6, "a worn square safety pictogram plate: a breathing mask, "
          "white on blue", y=1.8 ),
    wall( "graffiti_1", "industrial street", "white", 2.2, "spray painted graffiti, abstract wild-style tag "
          "shapes without readable letters, neon pink and cyan, drips", y=1.3, aspect="16:9" ),
    wall( "graffiti_2", "industrial street", "white", 2.0, "spray painted graffiti of a grinning skull with a "
          "crown, black and orange", y=1.3, aspect="16:9" ),
    wall( "graffiti_3", "street", "white", 2.2, "a spray painted mural of a whale swimming among stars, blue "
          "and gold", y=1.4, aspect="16:9" ),
    wall( "graffiti_4", "industrial street", "white", 1.6, "spray painted tally marks and a crossed-out eye "
          "symbol, red", y=1.3, aspect="4:3" ),
    wall( "fusebox_1", "industrial", "white", 0.8, "a grey electrical junction box with conduit pipes and a warning "
          "stripe", y=1.5, aspect="3:4" ),
    wall( "fusebox_2", "industrial clean", "white", 0.8, "a wall-mounted fire extinguisher cabinet with a red "
          "handle", y=1.2, aspect="3:4" ),
    wall( "schematic_1", "industrial", "none", 1.0, "a faded evacuation map on a metal plate, line drawing of "
          "corridors with invented glyph labels", y=1.6, aspect="4:3" ),
    wall( "schematic_2", "industrial clean", "none", 1.0, "a technical diagram of a reactor on a metal plate, "
          "line drawing, invented glyph labels", y=1.6, aspect="4:3" ),
    wall( "handprints_1", "industrial street", "white", 1.0, "grimy oily handprints", y=1.2 ),
    wall( "neon_work_1", "industrial", "black", 1.0, "a glowing orange neon tube sign of a pickaxe and a star",
          glow=3.0, y=2.2 ),
    wall( "neon_work_2", "industrial", "black", 1.2, "a glowing green neon tube arrow sign", glow=3.0, y=2.4,
          aspect="16:9" ),
    # Streets and bars.
    wall( "poster_ad_1", "street", "none", 1.25, "a colourful advertising poster for a noodle bar with a steaming "
          "bowl, retro sci-fi illustration", y=1.7, aspect="3:4" ),
    wall( "poster_ad_2", "street", "none", 1.25, "a travel poster for a holiday on a water world with two suns, "
          "retro sci-fi illustration", y=1.7, aspect="3:4" ),
    wall( "poster_ad_3", "street", "none", 1.25, "a poster for a fighting tournament, two armoured fighters, "
          "retro sci-fi illustration", y=1.7, aspect="3:4" ),
    wall( "poster_ad_4", "street", "none", 1.25, "an advert for a new cybernetic arm, glossy chrome, retro sci-fi "
          "illustration", y=1.7, aspect="3:4" ),
    wall( "poster_ad_5", "street", "none", 1.25, "a lottery poster with a shower of golden coins, retro sci-fi "
          "illustration", y=1.7, aspect="3:4" ),
    wall( "poster_ad_6", "street", "none", 1.25, "a concert poster of a singer with a glowing microphone, retro "
          "sci-fi illustration", y=1.7, aspect="3:4" ),
    wall( "wanted_1", "street industrial", "none", 1.1, "a wanted poster with a sketched face of a scarred spacer",
          y=1.7, aspect="3:4" ),
    wall( "wanted_2", "street", "none", 1.1, "a missing person poster with a faded photo of a smiling woman",
          y=1.7, aspect="3:4" ),
    wall( "neon_bar_1", "street", "black", 1.0, "a glowing pink neon tube sign shaped like a cocktail glass",
          glow=3.5, y=2.4 ),
    wall( "neon_bar_2", "street", "black", 1.0, "a glowing orange neon tube sign of a noodle bowl with chopsticks",
          glow=3.5, y=2.4 ),
    wall( "neon_bar_3", "street", "black", 1.0, "a glowing blue neon tube sign of a planet with a ring",
          glow=3.5, y=2.4 ),
    wall( "neon_bar_4", "street", "black", 1.0, "a glowing green neon tube sign of a cat face", glow=3.5, y=2.4 ),
    wall( "menu_1", "street", "none", 1.2, "a chalkboard menu with chalk drawings of dishes", y=1.6, aspect="3:4" ),
    # Clean halls.
    wall( "medposter_1", "clean", "none", 1.1, "a clean medical poster with an anatomy diagram", y=1.7,
          aspect="3:4" ),
    wall( "medposter_2", "clean", "none", 1.1, "a clean hygiene poster with hand washing pictograms", y=1.7,
          aspect="3:4" ),
    wall( "medposter_3", "clean", "none", 1.1, "a calm poster of a nurse with a soft smile, pastel illustration",
          y=1.7, aspect="3:4" ),
    wall( "screen_1", "clean", "black", 1.2, "a glowing wall display showing a star map", glow=1.8, y=1.8,
          aspect="16:9" ),
    wall( "screen_2", "clean industrial", "black", 1.2, "a glowing wall display with a ship status diagram",
          glow=1.8, y=1.8, aspect="16:9" ),
    wall( "screen_3", "clean", "black", 1.0, "a glowing wall display with green heartbeat lines", glow=1.8,
          y=1.8, aspect="16:9" ),
    wall( "pictogram_1", "clean", "none", 0.5, "a minimalist wayfinding plate with an arrow and a bed symbol",
          y=1.9 ),
    wall( "pictogram_2", "clean", "none", 0.5, "a minimalist wayfinding plate with a stair symbol and an arrow",
          y=1.9 ),
    wall( "banner_1", "clean", "none", 1.8, "a vertical banner with an abstract golden emblem on deep blue cloth",
          y=2.0, aspect="9:16" ),
    wall( "banner_2", "clean street", "none", 1.8, "a vertical banner with a white star on deep red cloth", y=2.0,
          aspect="9:16" ),
    # Cold places.
    wall( "frostwall_1", "cold", "black", 1.6, "white feathery frost bloom spreading", y=1.6 ),
    wall( "frostwall_2", "cold", "black", 1.4, "frost crusted around a bolted seam", y=1.4 ),
    wall( "frostwall_3", "cold", "black", 1.2, "a frozen stream of drips turned to ice", y=2.0, aspect="3:4" ),
    wall( "icicles_1", "cold", "black", 1.6, "a row of hanging icicles", y=2.8, aspect="16:9" ),
    wall( "icicles_2", "cold", "black", 1.2, "a few long hanging icicles", y=2.8, aspect="4:3" ),
    # Outdoors.
    wall( "rockpaint_1", "outdoor", "white", 1.6, "ancient ochre rock painting of figures and suns", y=1.5,
          aspect="4:3" ),
    wall( "rockpaint_2", "outdoor alien", "white", 1.4, "ancient carved spiral symbols", y=1.5 ),
    wall( "vein_1", "outdoor industrial", "black", 1.4, "a glittering vein of gold-coloured ore", y=1.4 ),
    wall( "vein_2", "outdoor", "black", 1.4, "a cluster of pale blue crystals", glow=0.6, y=1.0 ),
    # The object of Tesh.
    wall( "glyphwall_1", "alien", "black", 1.4, "a glowing alien inscription, columns of cyan geometric symbols",
          glow=2.5, y=1.8, aspect="3:4" ),
    wall( "glyphwall_2", "alien", "black", 1.6, "a glowing violet alien circle of runes", glow=2.5, y=1.8 ),
    wall( "glyphwall_3", "alien", "black", 1.6, "faint glowing white alien lines like a circuit", glow=1.6,
          y=1.8 ),
    wall( "glyphwall_4", "alien", "black", 1.2, "a glowing teal alien eye symbol", glow=2.2, y=2.0 ),
    wall( "crystal_1", "alien", "black", 1.2, "a crystal cluster growing out, glowing violet", glow=1.4, y=1.0 ),
    wall( "crystal_2", "alien", "black", 1.0, "a crystal cluster growing out, glowing pale cyan", glow=1.4,
          y=1.0 ),
    # Gardens.
    wall( "ivy_1", "garden", "white", 1.6, "climbing ivy vines with leaves", y=1.8, aspect="3:4" ),
    wall( "ivy_2", "garden street", "white", 1.4, "a flowering climbing vine with small purple flowers", y=1.8,
          aspect="3:4" ),
    wall( "ivy_3", "garden", "white", 1.6, "hanging moss and trailing vines", y=2.4, aspect="16:9" ),
    wall( "moss_1", "garden outdoor", "white", 1.2, "green moss and lichen patches", y=0.8 ),
    wall( "moss_2", "garden", "white", 1.0, "a small birdhouse with a round hole", y=2.0 ),
]


def variant_jobs():
    for base, hints in VARIANTS.items():
        if base not in make_textures.TEXTURES:
            continue
        kind, aspect, prompt, metallic, roughness, strength = make_textures.TEXTURES[base]
        for n, hint in enumerate( hints, 1 ):
            text = prompt.replace( make_textures.FLAT, "this variant " + hint + ", the same palette, scale and "
                                   "style as the plain material, " + make_textures.FLAT )
            yield ( "%s_v%d" % ( base, n ), "variant", ( base, text, metallic, roughness, strength ) )


def match_colours( name, base ):
    """Pulls the variant's albedo towards the plain one's mean and spread (the patchwork stays one material)."""
    path = os.path.join( OUT, name + "_albedo.png" )
    a = np.asarray( Image.open( path ).convert( "RGB" ), dtype=np.float64 )
    b = np.asarray( Image.open( os.path.join( OUT, base + "_albedo.png" ) ).convert( "RGB" ), dtype=np.float64 )
    am, asd = a.mean( axis=( 0, 1 ) ), a.std( axis=( 0, 1 ) ) + 1e-3
    bm, bsd = b.mean( axis=( 0, 1 ) ), b.std( axis=( 0, 1 ) )
    matched = ( a - am ) / asd * bsd + bm
    out = a * 0.3 + matched * 0.7
    Image.fromarray( np.clip( out + 0.5, 0, 255 ).astype( np.uint8 ) ).save( path )


def key_decal( d, raw, out ):
    """Keys the background out (white: dark things unblended from white; black: bright things), softens the edges."""
    img = np.asarray( Image.open( raw ).convert( "RGB" ), dtype=np.float64 ) / 255.0
    h, w = img.shape[:2]
    scale = 512.0 / max( h, w )
    img = np.asarray( Image.fromarray( ( img * 255 ).astype( np.uint8 ) ).resize(
        ( max( 1, int( w * scale ) ), max( 1, int( h * scale ) ) ), Image.LANCZOS ), dtype=np.float64 ) / 255.0
    h, w = img.shape[:2]
    if d["key"] == "none":
        # A plate or a poster: cut away the margin the model likes to paint around it.
        border = np.median( np.concatenate( [img[0], img[-1], img[:, 0], img[:, -1]] ), axis=0 )
        differs = np.abs( img - border ).max( axis=-1 ) > 0.1
        rows, cols = np.where( differs.any( axis=1 ) )[0], np.where( differs.any( axis=0 ) )[0]
        if len( rows ) > h * 0.3 and len( cols ) > w * 0.3:
            img = img[rows[0]:rows[-1] + 1, cols[0]:cols[-1] + 1]
            h, w = img.shape[:2]
    if d["key"] == "white":
        dist = ( 1.0 - img ).max( axis=-1 )
        alpha = np.clip( ( dist - 0.06 ) / 0.5, 0.0, 1.0 )
        colour = np.clip( ( img - ( 1.0 - dist[..., None] ) ) / np.maximum( dist, 0.06 )[..., None], 0.0, 1.0 )
    elif d["key"] == "black":
        dist = img.max( axis=-1 )
        alpha = np.clip( ( dist - 0.06 ) / 0.45, 0.0, 1.0 )
        colour = np.clip( img / np.maximum( dist, 0.06 )[..., None], 0.0, 1.0 )
    else:
        # Opaque, except a plain margin reached from the edge (the paper and the tape stay).
        border = np.median( np.concatenate( [img[0], img[-1], img[:, 0], img[:, -1]] ), axis=0 )
        plain = np.abs( img - border ).max( axis=-1 ) < 0.09
        if not ( border.min() > 0.85 or border.max() < 0.12 ):
            plain[:] = False   # the edge is the poster itself
        outside = np.zeros( ( h, w ), dtype=bool )
        outside[0], outside[-1], outside[:, 0], outside[:, -1] = plain[0], plain[-1], plain[:, 0], plain[:, -1]
        while True:
            grown = outside.copy()
            grown[1:] |= outside[:-1]
            grown[:-1] |= outside[1:]
            grown[:, 1:] |= outside[:, :-1]
            grown[:, :-1] |= outside[:, 1:]
            grown &= plain
            if ( grown == outside ).all():
                break
            outside = grown
        alpha = 1.0 - outside.astype( np.float64 )
        soft = alpha.copy()
        soft[1:-1, 1:-1] = ( alpha[1:-1, 1:-1] * 4 + alpha[:-2, 1:-1] + alpha[2:, 1:-1] + alpha[1:-1, :-2] +
                             alpha[1:-1, 2:] ) / 8.0
        alpha = soft
        colour = img
    # Nothing reaches the edge of the quad (a keyed one fades out, a poster keeps a hairline).
    yy = np.minimum( np.arange( h ), np.arange( h )[::-1] )[:, None] / ( h * 0.06 )
    xx = np.minimum( np.arange( w ), np.arange( w )[::-1] )[None, :] / ( w * 0.06 )
    edge = np.clip( np.minimum( yy, xx ), 0.0, 1.0 )
    alpha = alpha * ( edge if d["key"] != "none" else np.clip( np.minimum( yy, xx ) * 6.0, 0.0, 1.0 ) )
    rgba = np.dstack( [colour, alpha] )
    Image.fromarray( ( rgba * 255 + 0.5 ).astype( np.uint8 ), "RGBA" ).save( out )
    # The mean colour where it shows (the colour of its light, if it glows).
    weight = alpha.sum() + 1e-6
    return ( colour * alpha[..., None] ).sum( axis=( 0, 1 ) ) / weight, w / h


def write_list():
    """assets/textures/decals/decals.txt from the decals that are there."""
    lines = ["# made by tools/make_dress.py: group <name> <themes...>",
             "# decal <name> <floor|wall> <width m> <height m> <centre height m> <roughness> <glow> <a|m: alpha or "
             "multiply> <r> <g> <b> <groups...>"]
    for g, themes in GROUPS.items():
        lines.append( "group %s %s" % ( g, themes ) )
    for d in DECAL_LIST:
        path = os.path.join( DECALS, d["name"] + ".png" )
        if not os.path.exists( path ):
            continue
        img = np.asarray( Image.open( path ), dtype=np.float64 ) / 255.0
        alpha = img[..., 3]
        mean = ( img[..., :3] * alpha[..., None] ).sum( axis=( 0, 1 ) ) / ( alpha.sum() + 1e-6 )
        aspect = img.shape[1] / img.shape[0]
        width = d["size"] if aspect >= 1.0 else d["size"] * aspect
        height = width / aspect
        lines.append( "decal %s %s %.2f %.2f %.2f %.2f %.1f %s %.2f %.2f %.2f %s" % (
            d["name"], d["where"], width, height, d["y"], d["rough"], d["glow"], "m" if d["mul"] else "a", mean[0],
            mean[1], mean[2], " ".join( d["groups"] ) ) )
    # The gardens' paths (tools/make_garden.py) belong to no place: the level lays them along its way.
    for name in ( "path_1", "path_2" ):
        if os.path.exists( os.path.join( DECALS, name + ".png" ) ):
            lines.append( "decal %s floor 2.60 2.60 1.60 0.90 0.0 a 0.30 0.25 0.18" % name )
    with open( os.path.join( DECALS, "decals.txt" ), "w", encoding="utf-8", newline="\n" ) as f:
        f.write( "\n".join( lines ) + "\n" )
    return len( lines ) - 2 - len( GROUPS )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument( "--force", nargs="*", default=[] )
    ap.add_argument( "--only", nargs="*", default=None )
    ap.add_argument( "--part", default=None, help="k/n: only every n-th job from the k-th (runs side by side)" )
    ap.add_argument( "--list", action="store_true", help="only write decals.txt from what is there" )
    ap.add_argument( "--local", action="store_true", help="paint the missing ones locally (FLUX.2), not on Vertex" )
    args = ap.parse_args()
    os.makedirs( DECALS, exist_ok=True )
    os.makedirs( RAWDIR, exist_ok=True )
    if args.list:
        print( "decals listed: %d" % write_list() )
        return 0
    # Variants and decals in turns (both grow while it runs).
    variants = list( variant_jobs() )
    decals = [( d["name"], "decal", d ) for d in DECAL_LIST]
    jobs = []
    for i in range( max( len( variants ), len( decals ) ) ):
        jobs += variants[i:i + 1] + decals[i:i + 1]
    if args.part:
        k, n = ( int( v ) for v in args.part.split( "/" ) )
        jobs = jobs[k::n]
    if args.local:
        # All missing pictures in one local batch first; the loop below then only keys and maps them.
        for name in args.force:
            raw = os.path.join( RAWDIR, name + ".jpg" )
            if os.path.exists( raw ):
                os.remove( raw )
        todo = []
        for name, kind, job in jobs:
            if args.only is not None and name not in args.only:
                continue
            raw = os.path.join( RAWDIR, name + ".jpg" )
            todo.append( ( raw, "1:1", job[1] ) if kind == "variant" else ( raw, job["aspect"], job["prompt"] ) )
        print( "painted locally: %d" % make_textures.paint_local( todo, "dress" ), flush=True )
    for name, kind, job in jobs:
        if args.only is not None and name not in args.only:
            continue
        raw = os.path.join( RAWDIR, name + ".jpg" )
        try:
            if not os.path.exists( raw ) or ( name in args.force and not args.local ):
                if args.local:
                    raise RuntimeError( "not painted" )
                if kind == "variant":
                    make_textures.generate( "dress/" + name, "1:1", job[1] )
                else:
                    make_textures.generate( "dress/" + name, job["aspect"], job["prompt"] )
            if kind == "variant":
                if not os.path.exists( os.path.join( OUT, name + "_albedo.png" ) ) or name in args.force:
                    base, text, metallic, roughness, strength = job
                    make_textures.tile_maps( name, raw, metallic, roughness, strength )
                    match_colours( name, base )
            else:
                out = os.path.join( DECALS, name + ".png" )
                if not os.path.exists( out ) or name in args.force:
                    key_decal( job, raw, out )
        except Exception as e:  # noqa: BLE001  (one failure must not stop the rest)
            print( f"{name}: failed ({e})", flush=True )
            continue
        print( f"{name}: done", flush=True )
    print( "decals listed: %d" % write_list() )
    return 0


if __name__ == "__main__":
    sys.exit( main() )
