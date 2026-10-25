"""Generates the look test's textures with Gemini's image model (Vertex AI, the PC's gcloud ADC) and derives the maps the
renderer reads. (Imagen 3 and 4 are gone from Vertex: 404.)

    G:\\Tools\\ImageGeneration\\venv\\Scripts\\python.exe tools\\make_textures.py [--force NAME ...] [--only NAME ...]

Each picture is asked for once and kept in assets/raw/<name>.jpg (the source; --force asks again). From it:
  tiles     <name>_albedo.png  seamless (opposite edges cross-faded), baked light removed
            <name>_normal.png  OpenGL convention (green up), from the luminance as a height field
            <name>_orm.png     R ambient occlusion, G roughness, B metallic
  pictures  <name>.png         as generated (sky, planet, screens)
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "assets", "raw")
OUT = os.path.join(ROOT, "assets", "textures")
PROJECT = "plenary-office-432719-u6"
LOCATIONS = ["global", "us-central1"]
MODELS = ["gemini-3-pro-image", "gemini-3.1-flash-image"]

FLAT = ("flat uniform lighting, no shadows, no perspective, no text, no logos, "
        "photorealistic PBR material scan, highly detailed")

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
                              "imageConfig": { "aspectRatio": aspect, "imageSize": "2K" } },
    } ).encode()
    last = None
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
    args = ap.parse_args()
    os.makedirs( RAW, exist_ok=True )
    os.makedirs( OUT, exist_ok=True )
    for name, (kind, aspect, prompt, metallic, roughness, strength) in TEXTURES.items():
        if args.only and name not in args.only:
            continue
        raw = os.path.join( RAW, name + ".jpg" )
        if not os.path.exists( raw ) or name in args.force:
            generate( name, aspect, prompt )
        if kind == "tile":
            tile_maps( name, raw, metallic, roughness, strength )
        else:
            Image.open( raw ).convert( "RGB" ).save( os.path.join( OUT, name + ".png" ) )
        print( f"{name}: done" )
    return 0


if __name__ == "__main__":
    sys.exit( main() )
