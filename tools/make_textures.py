"""Generates the look test's textures with Gemini's image model (Vertex AI, the PC's gcloud ADC) and derives the maps the
renderer reads. (Imagen 3 and 4 are gone from Vertex: 404.)

    G:\\Tools\\ImageGeneration\\venv\\Scripts\\python.exe tools\\make_textures.py [--force NAME ...] [--only NAME ...]
        [--lang CODE ...]

Each picture is asked for once and kept in assets/raw/<name>.jpg (the source; --force asks again). From it:
  tiles     <name>_albedo.png  seamless (opposite edges cross-faded), baked light removed
            <name>_normal.png  OpenGL convention (green up), from the luminance as a height field
            <name>_orm.png     R ambient occlusion, G roughness, B metallic
  pictures  <name>.png         as generated (sky, planet, screens)
  signs     <name>.<lang>.png  one per language: every "sign_... = TOP | BOTTOM" in assets/text/<lang>/*.txt
                               (--lang makes only the signs of these languages; --force sign_k7 asks again)
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "assets", "raw")
OUT = os.path.join(ROOT, "assets", "textures")
TEXT = os.path.join(ROOT, "assets", "text")
PROJECT = "plenary-office-432719-u6"
LOCATIONS = ["global", "us-central1"]
MODELS = ["gemini-3-pro-image", "gemini-3.1-flash-image"]
IMAGE_SIZE = "2K"   # make_items.py and make_dress.py ask for 1K (their pictures end up small)

FLAT = ("flat uniform lighting, no shadows, no perspective, no text, no logos, "
        "photorealistic PBR material scan, highly detailed")

SIGN = ("A weathered industrial sign plate on a space station wall, flat straight front view filling the whole frame, "
        "dark grey painted steel with a rivet in each corner, large white stencilled block letters reading exactly "
        "\"{}\", below them a smaller line reading exactly \"{}\", a yellow and black hazard stripe along the bottom "
        "edge, scratches, grime and rust streaks, no other text, no logos, no wall around it")

# name: (kind, aspect, prompt, metallic, roughness, normal strength)
TEXTURES = {
    "floor": ("tile", "1:1",
              "Seamless tileable texture, top-down orthographic view of worn industrial spaceship deck plating: large "
              "square riveted dark steel floor plates with a diamond-tread anti-slip pattern, narrow seams between the "
              "plates, grime, oil stains, scratches, chipped faded yellow paint along a few plate edges, " + FLAT,
              0.85, 0.55, 3.0),
    "wall": ("tile", "1:1",
             "Seamless tileable texture, orthographic front view of a sci-fi space station bulkhead wall: modular dark "
             "gunmetal panels with deep recessed seams, small ventilation grilles, rows of bolts, rust streaks and "
             "grime running down, " + FLAT,
             0.75, 0.6, 3.5),
    "metal": ("tile", "1:1",
              "Seamless tileable texture, dark brushed steel plate with fine scratches, weld seams, worn edges and "
              "light grime, " + FLAT,
              1.0, 0.4, 1.5),
    "hazard": ("tile", "1:1",
               "Seamless tileable texture, worn diagonal black and yellow hazard stripes painted on scratched steel, "
               "chipped paint showing bare metal, grime, " + FLAT,
               0.5, 0.6, 2.0),
    "pipes": ("tile", "1:1",
              "Seamless tileable texture, orthographic front view of a bundle of thick horizontal industrial pipes and "
              "conduits with clamps, flanges and small valves on a dark metal wall, grime, " + FLAT,
              0.9, 0.5, 5.0),
    "crate": ("tile", "1:1",
              "Seamless tileable texture, chipped and scratched faded orange industrial paint on steel, dents, scuffs, "
              "exposed bare metal at the scratches, grime, " + FLAT,
              0.4, 0.55, 2.5),
    "armor": ("tile", "1:1",
              "Seamless tileable texture, scuffed dark grey ceramic composite armor plating with fine panel lines, "
              "worn edges, small scratches and dust, " + FLAT,
              0.3, 0.45, 2.0),
    "obsidian": ("tile", "1:1",
                 "Seamless tileable texture, polished black obsidian stone with deeply carved alien glyphs and "
                 "geometric runes, subtle violet sheen, " + FLAT,
                 0.0, 0.15, 4.0),
    "rock": ("tile", "1:1",
             "Seamless tileable texture, orthographic front view of rough nickel-iron asteroid rock face cut by "
             "mining tools, dark grey with metallic glints, drill marks, cracks, frost in the crevices, " + FLAT,
             0.35, 0.75, 4.0),
    "grate": ("tile", "1:1",
              "Seamless tileable texture, top-down orthographic view of a rusty industrial steel floor grating with "
              "deep dark gaps between the bars, heavy corrosion, condensation droplets, grime, " + FLAT,
              0.8, 0.6, 5.0),
    "rust": ("tile", "1:1",
             "Seamless tileable texture, heavily rusted and pitted steel plating with flaking orange-brown rust, "
             "patches of old yellow paint, scratches, oily grime, " + FLAT,
             0.5, 0.7, 2.5),
    # The prologue's ship: container walls, refugees' clothes, the red dwarf, and the clinic of the last scene.
    "container": ("tile", "1:1",
                  "Seamless tileable texture, orthographic front view of the corrugated steel wall of a shipping "
                  "container, evenly spaced vertical trapezoid ribs, faded light grey paint, scratches, dents, rust "
                  "streaks running down, grime, " + FLAT,
                  0.6, 0.55, 4.0),
    "fabric": ("tile", "1:1",
               "Seamless tileable texture, worn quilted thermal fabric of a padded jacket, light grey, stitched quilting "
               "pattern, stains, frayed seams, close-up, " + FLAT,
               0.0, 0.85, 2.0),
    "reddwarf": ("picture", "16:9",
                 "Equirectangular texture map of the surface of a red dwarf star filling the whole frame as a flat "
                 "projection, deep red and orange plasma granulation cells, dark sunspot groups, bright faculae, "
                 "glowing filaments, highly detailed, no sphere, no space background, no text",
                 0.0, 0.0, 0.0),
    "clinic": ("picture", "16:9",
               "Cinematic still, a poor charity clinic inside a rotating space station habitat, a cramped ward with "
               "patched curtains between worn cots, salvaged medical machines with small amber screens, a round "
               "window showing stars and the curve of the habitat, dim warm light, dust in the air, exhaustion and "
               "kindness, painterly realism, no people, no text",
               0.0, 0.0, 0.0),
    "screen": ("picture", "16:9",
               "Retro-futuristic spaceship computer terminal screen filling the whole frame, amber and cyan vector "
               "graphics, orbital diagrams, waveform readouts, warning glyphs, dark background, glowing CRT phosphor, "
               "scanlines, flat front view, no bezel",
               0.0, 0.0, 0.0),
    "nebula": ("picture", "16:9",
               "Deep space vista filling the whole frame, a vast violet and teal nebula with dark dust lanes, dense "
               "starfield, a few bright blue stars, a distant spiral galaxy, awe-inspiring, cinematic, photorealistic "
               "astrophotography, no planets, no text",
               0.0, 0.0, 0.0),
    # The later places' walls and floors (Game::loadSurfaces maps them to themes; a level may choose its own).
    # Act I: the drum's alleys, the hull, the Core Bank, the drowned quarter.
    "plaster": ("tile", "1:1",
                "Seamless tileable texture, orthographic front view of a weathered house facade inside a space habitat: "
                "cracked and patched plaster in faded ochre and grey, water stains running down, soot, small repaired "
                "patches of sheet metal, " + FLAT,
                0.0, 0.85, 3.0),
    "streetfloor": ("tile", "1:1",
                    "Seamless tileable texture, top-down orthographic view of a worn street in a space habitat: "
                    "patched concrete slabs and steel plates, narrow drain channels, grime, oil stains, chewing gum "
                    "spots, scuffs, " + FLAT,
                    0.2, 0.8, 2.5),
    "hullplate": ("tile", "1:1",
                  "Seamless tileable texture, top-down orthographic view of a space station's outer hull: large light "
                  "grey heat-resistant ceramic plates in a grid, stencilled tiny markings without legible text, "
                  "micrometeorite pits, scorch marks, " + FLAT,
                  0.3, 0.6, 2.5),
    "vaultwall": ("tile", "1:1",
                  "Seamless tileable texture, orthographic front view of a clean bank vault wall: smooth pale ivory "
                  "ceramic panels with soft rounded seams, small recessed status slots, faint fingerprints, " + FLAT,
                  0.1, 0.35, 1.5),
    "vaultfloor": ("tile", "1:1",
                   "Seamless tileable texture, top-down orthographic view of a polished pale grey stone floor with "
                   "thin inlaid brass lines forming large squares, faint reflections, scuffs, " + FLAT,
                   0.2, 0.2, 1.2),
    "algae": ("tile", "1:1",
              "Seamless tileable texture, orthographic front view of a wall that was under water for years: corroded "
              "steel panels covered in green and brown algae, mineral crusts, white salt lines, wet sheen, " + FLAT,
              0.3, 0.55, 3.5),
    "wetfloor": ("tile", "1:1",
                 "Seamless tileable texture, top-down orthographic view of a flooded metal floor drying out: puddles, "
                 "silt, green slime in the seams of the plates, rust blooms, wet reflections, " + FLAT,
                 0.5, 0.35, 2.5),
    # Act II: ice, snow, dust and desert rock, the refinery's deck, the ark's sleeper halls.
    "iceblock": ("tile", "1:1",
                 "Seamless tileable texture, orthographic front view of a wall of rough glacial blue ice with pressure "
                 "cracks, trapped bubbles and white frost in the crevices, " + FLAT,
                 0.0, 0.25, 3.0),
    "snowfield": ("tile", "1:1",
                  "Seamless tileable texture, top-down orthographic view of wind-packed snow with fine ripples and "
                  "sastrugi, bluish shadows in the hollows, no footprints, " + FLAT,
                  0.0, 0.8, 2.0),
    "dust": ("tile", "1:1",
             "Seamless tileable texture, top-down orthographic view of dry ochre desert dust with small pebbles, "
             "fine cracks and wind ripples, " + FLAT,
             0.0, 0.9, 2.5),
    "dustrock": ("tile", "1:1",
                 "Seamless tileable texture, orthographic front view of a weathered red-brown sandstone cliff face with "
                 "horizontal strata, wind-carved hollows, dust, " + FLAT,
                 0.0, 0.85, 4.0),
    "deckrust": ("tile", "1:1",
                 "Seamless tileable texture, top-down orthographic view of a refinery deck: rust-red painted steel "
                 "plates with raised rivets, worn walkways showing bare metal, chemical stains, " + FLAT,
                 0.7, 0.6, 3.0),
    "podwall": ("tile", "1:1",
                "Seamless tileable texture, orthographic front view of an old colony ship's sleeper hall wall: white "
                "padded panels yellowed by centuries, soft quilted seams, small blue status lenses, frost in the "
                "corners, " + FLAT,
                0.1, 0.6, 2.0),
    # Act III: the ring's gardens, the clinic, the arena, the gate's yard, the hearing, the computing moon, the prison.
    "grass": ("tile", "1:1",
              "Seamless tileable texture, top-down orthographic view of a lush trimmed lawn, short green grass with a "
              "little clover and a few small daisies, " + FLAT,
              0.0, 0.9, 2.0),
    "hedge": ("tile", "1:1",
              "Seamless tileable texture, orthographic front view of a dense neatly trimmed boxwood hedge, small dark "
              "green leaves, " + FLAT,
              0.0, 0.9, 4.0),
    "tiles": ("tile", "1:1",
              "Seamless tileable texture, clean white ceramic tiles of a modern clinic with thin pale grey grout, a "
              "subtle pearly sheen, almost no wear, " + FLAT,
              0.0, 0.25, 1.5),
    "sand": ("tile", "1:1",
             "Seamless tileable texture, top-down orthographic view of fine golden arena sand, raked, with footprints "
             "and drag marks, " + FLAT,
             0.0, 0.95, 2.5),
    "sandstone": ("tile", "1:1",
                  "Seamless tileable texture, orthographic front view of an arena wall of large carved sandstone "
                  "blocks, weathered edges, faint red pigment left in the carvings, " + FLAT,
                  0.0, 0.85, 4.0),
    "yellowplate": ("tile", "1:1",
                    "Seamless tileable texture, yellow painted construction plating of a giant space structure, "
                    "scratched, stencilled grid lines without text, welding seams, grey primer showing through, " + FLAT,
                    0.6, 0.5, 3.0),
    "marble": ("tile", "1:1",
               "Seamless tileable texture, top-down orthographic view of polished dark grey veined stone floor slabs "
               "cut from an asteroid, thin seams, white veins, " + FLAT,
               0.0, 0.15, 1.5),
    "frost": ("tile", "1:1",
              "Seamless tileable texture, pale blue metal panels covered in white hoarfrost and feathery ice crystals, "
              "seams between the panels, " + FLAT,
              0.5, 0.6, 3.0),
    "scorched": ("tile", "1:1",
                 "Seamless tileable texture, heat-scorched blackened steel plates with rainbow temper colours, soot, "
                 "blistered paint and warped seams, " + FLAT,
                 0.8, 0.55, 3.0),
    # Act IV: Brannt being peeled, the Mill's furnace, its control room, the night shift's world.
    "regolith": ("tile", "1:1",
                 "Seamless tileable texture, top-down orthographic view of grey lunar regolith dust with small rocks, "
                 "tiny craters and boot-free undisturbed surface, " + FLAT,
                 0.0, 0.95, 3.0),
    "slag": ("tile", "1:1",
             "Seamless tileable texture, orthographic front view of a wall of dark volcanic slag and cooled metal with "
             "thin glowing orange veins in the cracks, " + FLAT,
             0.2, 0.85, 4.0),
    "serverfloor": ("tile", "1:1",
                    "Seamless tileable texture, top-down orthographic view of dark grey raised data centre floor tiles, "
                    "every other tile a perforated vent panel, " + FLAT,
                    0.6, 0.5, 2.0),
    "serverwall": ("tile", "1:1",
                   "Seamless tileable texture, orthographic front view of dark server rack fronts with rows of tiny "
                   "green and blue indicator lights, cable bundles, ventilation mesh, " + FLAT,
                   0.6, 0.5, 3.0),
    "netgrid": ("tile", "1:1",
                "Seamless tileable texture, a dark blue-black floor with a faint glowing cyan grid of thin lines, soft "
                "digital noise and a few flickering cells, " + FLAT,
                0.1, 0.4, 1.0),
    "netwall": ("tile", "1:1",
                "Seamless tileable texture, a pale dreamlike wall of softly glowing translucent panels, faded "
                "photographs of kitchens and classrooms dissolving into light, abstract, no text, no faces, " + FLAT,
                0.0, 0.5, 1.0),
    "brick": ("tile", "1:1",
              "Seamless tileable texture, orthographic front view of a rain-soaked brick tenement wall on a poor "
              "mining moon: dark wet red-brown bricks, streaks of water and soot, a few patched bricks, drainpipe "
              "rust stains, " + FLAT,
              0.0, 0.7, 2.5),
    "wetcobble": ("tile", "1:1",
                  "Seamless tileable texture, top-down orthographic view of wet dark cobblestones in a narrow alley at "
                  "night, shallow puddles reflecting faint cold light, grime in the gaps, " + FLAT,
                  0.1, 0.35, 2.5),
    "toolwall": ("tile", "1:1",
                 "Seamless tileable texture, orthographic front view of an industrial maintenance workshop wall: grey "
                 "perforated pegboard panels with hanging tools, cable reels, small labelled drawers without legible "
                 "text, scuffed paint, " + FLAT,
                 0.5, 0.6, 2.5),
    "ochrerock": ("tile", "1:1",
                  "Seamless tileable texture, orthographic front view of a mine tunnel wall on a heavy mining world: "
                  "ochre and rust coloured rock, heavy dark timber and steel supports, drill marks, ore dust, " + FLAT,
                  0.0, 0.9, 2.5),
    "minefloor": ("tile", "1:1",
                  "Seamless tileable texture, top-down orthographic view of a mine floor: packed ochre ore dust, rusty "
                  "narrow rail tracks, boot prints, scattered rock chips, " + FLAT,
                  0.1, 0.85, 2.5),
    "farmsoil": ("tile", "1:1",
                 "Seamless tileable texture, top-down orthographic view of a small frontier farm yard: trampled dark "
                 "soil, tufts of pale grass, wooden planks laid as a path, a few puddles, " + FLAT,
                 0.0, 0.85, 2.5),
    "gasgiant": ("picture", "16:9",
                 "Equirectangular texture map of a gas giant's cloud tops filling the whole frame as a flat projection, "
                 "horizontal turbulent cloud bands in ochre, rust, cream and deep teal, swirling vortex storms, highly "
                 "detailed, no sphere, no stars, no space background, no text",
                 0.0, 0.0, 0.0),
}


def token():
    """An access token from the PC's application default credentials."""
    import google.auth
    import google.auth.transport.requests
    creds, _ = google.auth.default( scopes=["https://www.googleapis.com/auth/cloud-platform"] )
    creds.refresh( google.auth.transport.requests.Request() )
    return creds.token


def signs( languages ):
    """The sign plates per language: {(name, lang): (top, bottom)} from assets/text/<lang>/*.txt."""
    out = {}
    for lang in sorted( os.listdir( TEXT ) ):
        folder = os.path.join( TEXT, lang )
        if not os.path.isdir( folder ) or (languages and lang not in languages):
            continue
        for file in sorted( os.listdir( folder ) ):
            if not file.endswith( ".txt" ):
                continue
            with open( os.path.join( folder, file ), encoding="utf-8-sig" ) as f:
                for line in f:
                    key, eq, value = line.strip().partition( "=" )
                    key = key.strip()
                    if eq and key.startswith( "sign_" ) and not line.lstrip().startswith( "#" ):
                        top, _, bottom = value.partition( "|" )
                        out[(key, lang)] = (top.strip(), bottom.strip())
    return out


# Local painting (FLUX.2-dev on the GPU in WSL2, tools/flux_wsl.py in Text2Video's conda env "flux"): no quota,
# no cost.
WSL_PYTHON = "~/miniconda3/envs/flux/bin/python"
FLUX_SIZES = { "1:1": ( 1024, 1024 ), "3:4": ( 896, 1152 ), "4:3": ( 1152, 896 ), "16:9": ( 1344, 768 ),
               "9:16": ( 768, 1344 ), "21:9": ( 1536, 640 ) }


def paint_local( jobs, tag ):
    """Paints [(raw path, aspect, prompt), ...] locally in one batch (the model loads once); returns how many exist."""
    import json
    import subprocess
    jobs = [j for j in jobs if not os.path.exists( j[0] )]
    if not jobs:
        return 0
    tasks = []
    for path, aspect, prompt in jobs:
        w, h = FLUX_SIZES.get( aspect, ( 1024, 1024 ) )
        tasks.append( { "prompt": prompt, "out": path.replace( "\\", "/" ), "width": w, "height": h, "seed": 1,
                        "steps": 24 } )
    work = os.path.join( ROOT, "work", "flux" )
    os.makedirs( work, exist_ok=True )
    spec = os.path.join( work, tag + ".json" )
    with open( spec, "w", encoding="utf-8" ) as f:
        json.dump( tasks, f, indent=1 )
    def linux( p ):
        p = os.path.abspath( p ).replace( "\\", "/" )
        return "/mnt/%s%s" % ( p[0].lower(), p[2:] )
    subprocess.run( ["wsl.exe", "-d", "Ubuntu-22.04", "--", "bash", "-lc",
                     "cd /tmp && nice -n 10 %s %s %s" % ( WSL_PYTHON, linux( os.path.join( ROOT, "tools", "flux_wsl.py" ) ),
                                                          linux( spec ) )], check=False )
    return sum( 1 for path, _, _ in jobs if os.path.exists( path ) )


def generate( name, aspect, prompt ):
    """Asks the Gemini image model for one picture and keeps it in assets/raw/<name>.png; returns its path."""
    import base64
    import io
    import json
    import urllib.request
    path = os.path.join( RAW, name + ".jpg" )
    body = json.dumps( {
        "contents": [{ "role": "user", "parts": [{ "text": prompt }] }],
        "generationConfig": { "responseModalities": ["IMAGE"],
                              "imageConfig": { "aspectRatio": aspect, "imageSize": IMAGE_SIZE } },
    } ).encode()
    import time
    last = None
    # Too many requests (429) passes: then all models and places again, after a growing pause.
    for attempt in range( 5 ):
        if attempt:
            print( f"  {name}: waiting {30 * attempt} s" )
            time.sleep( 30 * attempt )
        for model_id in MODELS:
            for location in LOCATIONS:
                host = "aiplatform.googleapis.com" if location == "global" else f"{location}-aiplatform.googleapis.com"
                url = (f"https://{host}/v1/projects/{PROJECT}/locations/{location}/publishers/google/models/"
                       f"{model_id}:generateContent")
                req = urllib.request.Request( url, data=body, method="POST", headers={
                    "Authorization": "Bearer " + token(), "Content-Type": "application/json" } )
                try:
                    with urllib.request.urlopen( req, timeout=300 ) as resp:
                        answer = json.load( resp )
                    for part in answer["candidates"][0]["content"]["parts"]:
                        if "inlineData" in part:
                            data = base64.b64decode( part["inlineData"]["data"] )
                            Image.open( io.BytesIO( data ) ).convert( "RGB" ).save( path, quality=95 )
                            print( f"  {name}: {model_id} ({location})" )
                            return path
                    last = "no picture in the answer"
                except Exception as e:  # the next location or model
                    last = e
                    print( f"  {name}: {model_id} ({location}) failed: {str( e )[:160]}" )
    raise RuntimeError( f"{name}: no model answered ({last})" )


def blur( a, sigma ):
    """Gaussian blur with wrap-around (the tiles repeat), per channel, via the FFT."""
    h, w = a.shape[:2]
    fy = np.fft.fftfreq( h )[:, None]
    fx = np.fft.rfftfreq( w )[None, :]
    kernel = np.exp( -2.0 * (np.pi * sigma) ** 2 * (fx * fx + fy * fy) )
    if a.ndim == 2:
        return np.fft.irfft2( np.fft.rfft2( a ) * kernel, s=(h, w) )
    return np.stack( [np.fft.irfft2( np.fft.rfft2( a[..., c] ) * kernel, s=(h, w) ) for c in range( a.shape[2] )],
                     axis=-1 )


def seamless( a, band ):
    """Cross-fades each edge's last `band` pixels into the opposite edge and crops them: the result tiles."""
    def axis0( x ):
        n = x.shape[0]
        t = np.linspace( 0.0, 1.0, band, endpoint=False )[:, None, None]
        out = x[: n - band].copy()
        out[:band] = x[n - band:] * (1.0 - t) + x[:band] * t
        return out
    a = axis0( a )
    return np.swapaxes( axis0( np.swapaxes( a, 0, 1 ) ), 0, 1 )


def tile_maps( name, raw, metallic, roughness, strength ):
    """Albedo, normal and ORM maps of one tile."""
    img = np.asarray( Image.open( raw ).convert( "RGB" ), dtype=np.float64 ) / 255.0
    img = seamless( img, img.shape[0] // 8 )
    img = np.asarray( Image.fromarray( (img * 255).astype( np.uint8 ) ).resize( (1024, 1024), Image.LANCZOS ),
                      dtype=np.float64 ) / 255.0
    # Baked light: divide by the very large scale and bring back the mean.
    lum = img @ np.array( [0.2126, 0.7152, 0.0722] )
    low = blur( lum, 96.0 )
    albedo = np.clip( img * (lum.mean() / np.maximum( low, 0.02 ))[..., None], 0.0, 1.0 )
    # Height: the luminance with its large scale removed; the normal from its slopes.
    height = lum - blur( lum, 48.0 )
    height = blur( height, 1.0 )
    height /= max( np.abs( height ).max(), 1e-6 )
    dx = (np.roll( height, -1, axis=1 ) - np.roll( height, 1, axis=1 )) * 0.5
    dy = (np.roll( height, -1, axis=0 ) - np.roll( height, 1, axis=0 )) * 0.5
    n = np.stack( [-dx * strength, dy * strength, np.ones_like( height )], axis=-1 )
    n /= np.linalg.norm( n, axis=-1, keepdims=True )
    # Cavities darken (ambient occlusion) and roughen; saturated paint is no metal.
    cavity = np.clip( (blur( height, 6.0 ) - height) * 2.5, 0.0, 1.0 )
    ao = 1.0 - 0.7 * cavity
    sat = albedo.max( axis=-1 ) - albedo.min( axis=-1 )
    rough = np.clip( roughness + 0.35 * cavity + 0.25 * (0.5 - albedo.mean( axis=-1 )), 0.05, 1.0 )
    metal = np.clip( metallic * (1.0 - 2.0 * sat), 0.0, 1.0 )
    save = lambda arr, suffix: Image.fromarray( (np.clip( arr, 0, 1 ) * 255 + 0.5).astype( np.uint8 ) ).save(
        os.path.join( OUT, f"{name}_{suffix}.png" ) )
    save( albedo, "albedo" )
    save( n * 0.5 + 0.5, "normal" )
    save( np.stack( [ao, rough, metal], axis=-1 ), "orm" )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument( "--force", nargs="*", default=[], help="ask Imagen again for these" )
    ap.add_argument( "--only", nargs="*", default=None, help="only these textures" )
    ap.add_argument( "--lang", nargs="*", default=None, help="signs only in these languages" )
    args = ap.parse_args()
    os.makedirs( RAW, exist_ok=True )
    os.makedirs( OUT, exist_ok=True )
    for name, (kind, aspect, prompt, metallic, roughness, strength) in TEXTURES.items():
        if (args.only and name not in args.only) or args.lang is not None:
            continue
        raw = os.path.join( RAW, name + ".jpg" )
        if not os.path.exists( raw ) or name in args.force:
            generate( name, aspect, prompt )
        if kind == "tile":
            tile_maps( name, raw, metallic, roughness, strength )
        else:
            Image.open( raw ).convert( "RGB" ).save( os.path.join( OUT, name + ".png" ) )
        print( f"{name}: done" )
    # The signs, in every language (the story's world speaks each player's language).
    for (name, lang), (top, bottom) in signs( args.lang ).items():
        if args.only and name not in args.only:
            continue
        local = f"{name}.{lang}"
        raw = os.path.join( RAW, local + ".jpg" )
        if not os.path.exists( raw ) or name in args.force:
            generate( local, "21:9", SIGN.format( top, bottom ) )
        Image.open( raw ).convert( "RGB" ).save( os.path.join( OUT, local + ".png" ) )
        print( f"{local}: done" )
    return 0


if __name__ == "__main__":
    sys.exit( main() )
