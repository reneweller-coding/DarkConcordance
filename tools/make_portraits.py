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
    # Act II's speakers.
    "unzu": "A retired survey starship's mind: a large round sensor eye of amber glass set in a battered instrument "
            "console of scratched brass and steel, faint survey diagrams glowing beside it, dust, flat front view. "
            + STYLE,
    "funk": "An old trader's radio set on a ship's bulkhead: an amber display, chunky dials, a hanging microphone, "
            "stickers without legible words, flat front view. " + STYLE,
    "kern": "A balloon's control core: a fist-sized crystal of pale blue light in a cage of cables inside cramped "
            "gondola machinery, condensation, a sense of a mind inside it, no people, no hands, only the machine, flat front "
            "view. " + STYLE,
    "okafor": "Commander Okafor of a clearance squad, a woman of about forty-five with dark skin and close-cropped "
              "grey hair, a scar across her cheek, the olive collar of a heavy walker suit, tired and determined "
              "eyes. " + STYLE,
    "arche": "A colony ark's administration program seven hundred years old: an antique green phosphor terminal in a "
             "frosted white console, simple line drawings of sleeping figures on the screen, flat front view. "
             + STYLE,
    "pell": "An old researcher of about eighty with a thin white beard and wild white hair, layered worn thermal "
            "clothes, fingerless gloves, a thick notebook held to his chest, eyes full of wonder. " + STYLE,
    "hale": "Director Marit Hale, a woman in her sixties born in a mining settlement, a sharp lined face, short iron "
            "grey hair, an expensive dark suit with an old miner's badge on the lapel, hard intelligent eyes. "
            + STYLE,
    # Act III and IV.
    "berater": "A friendly clinic assistant: a smooth white ceramic face plate on a slender stand, two soft green "
               "lights for eyes, a calm curve for a mouth, a bright clean room behind. " + STYLE,
    "lamm": "Teodor Lamm, a celebrated arena champion of a utopian civilisation, handsome, golden blond, flawless "
            "skin, the crimson and gold collar of a ceremonial armour, a careless radiant smile. " + STYLE,
    "bedauern": "The avatar of a vast machine mind: a serene genderless face formed of soft white light and slowly "
                "drifting geometric particles, calm sorrowful eyes. " + STYLE,
    "sorge": "The avatar of a secret service's machine mind: a smooth featureless grey face like a mask made of "
             "thousands of tiny drones, a faint cold blue light, unsettling calm. " + STYLE,
    "ruul": "An engineer held prisoner for nine years, in his fifties, gaunt, shaved head, a faded prison overall, "
            "skin reddened by a hot star's light, intelligent haunted eyes. " + STYLE,
    "rigcore": "A drilling rig's control core on an ice moon: a frosted steel housing with a single tired amber light, "
               "icicles on its cables, a shift counter of worn digits beside it, flat front view, no people. " + STYLE,
    "lancecore": "A plasma lance's control core at a star lifter: a heat-scarred housing glowing faintly orange, a small "
                 "cracked screen with a waveform, sparks, flat front view, no people. " + STYLE,
    "zoegernd": "A mining cutter's control core: a scorched cockpit with a single cyan eye light behind cracked glass, "
                "tilted as if uncertain, dust and laser scars, flat front view, full bleed, no border, no white frame. " + STYLE,
    "vorarbeiter": "A foreman who is a stolen mind in a machine: a heavy industrial crane cab with a single red eye "
                   "light behind grimy glass, rust, warning paint, flat front view. " + STYLE,
    "ilka": "Ilka, a woman who looks about forty though she was a nine-year-old girl once, short dark hair, tired "
            "fierce eyes, simple grey work clothes, a faint digital shimmer at her edges as if she were not quite "
            "real. " + STYLE,
    # The heroes, for the character page.
    "corvin": "A lean man in his late thirties, short dark hair going grey at the temples, stubble, a small scar "
              "through one eyebrow, watchful tired eyes, the neck ring of a worn grey armoured hardsuit with an amber "
              "status light, a former raider who now takes escort jobs. " + STYLE,
    "dorra": "A short, very broad woman in her forties from a high-gravity world, thick neck and heavy shoulders, "
             "copper-red hair tied back tight, freckles, a once-broken nose, calm stubborn eyes, dented and welded "
             "ochre plates of an old mining suit. " + STYLE,
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
