"""Paints the portraits shown in conversations with Gemini's image model (same access as make_textures.py) and keeps
them as assets/textures/portrait_<name>.png (square, no text).

    G:\\Tools\\ImageGeneration\\venv\\Scripts\\python.exe tools\\make_portraits.py [--force NAME ...]
"""
import argparse
import os
import sys

from PIL import Image

sys.path.insert( 0, os.path.dirname( os.path.abspath( __file__ ) ) )
import make_textures  # noqa: E402  (generate(), the paths)

STYLE = ("painted portrait, head and shoulders, three-quarter view, dim warm light of a station bar, dark plain "
         "background, painterly realism with visible brush strokes, muted colours, grounded science fiction, no text, "
         "no logos, no frame")

PORTRAITS = {
    "ysolde": "An elegant, tired woman who looks about fifty, silver-streaked dark hair pinned up, high-collared dark "
              "violet coat, a thin silver chain, kind but unreadable eyes, a faint smile. " + STYLE,
    "ottla": "A broad older woman, landlady of a miners' bar, cropped grey hair, weathered face, a burn scar on her "
             "forearm, a worn union jacket under a stained apron, arms crossed, wary warmth. " + STYLE,
    "kiri": "A young mechanic woman with goggles pushed up into curly black hair, an oil-stained orange coverall, a "
            "mechanical left hand with exposed joints, a grin, a soldering tool behind her ear. " + STYLE,
    "hausmeister": "An old station administration terminal: a scratched amber monochrome screen in a heavy steel "
                   "housing, a simple pictogram of a building on the screen, warning labels without legible words, "
                   "flat front view. " + STYLE,
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument( "--force", nargs="*", default=[] )
    args = ap.parse_args()
    for name, prompt in PORTRAITS.items():
        key = "portrait_" + name
        raw = os.path.join( make_textures.RAW, key + ".jpg" )
        if not os.path.exists( raw ) or name in args.force:
            make_textures.generate( key, "1:1", prompt )
        Image.open( raw ).convert( "RGB" ).resize( (512, 512), Image.LANCZOS ).save(
            os.path.join( make_textures.OUT, key + ".png" ) )
        print( f"{key}: done" )
    return 0


if __name__ == "__main__":
    sys.exit( main() )
