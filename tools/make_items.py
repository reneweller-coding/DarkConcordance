"""Paints a picture for every kind of item, every unique and every set piece with Gemini's image model (the same access
as make_textures.py) and keeps them as assets/textures/items/<name>.png (square, 384 px), the originals in
assets/raw/items. The names follow Items.cpp: item_<kind>, unique_<n>, set_<n>.

    G:\\Tools\\ImageGeneration\\venv\\Scripts\\python.exe tools\\make_items.py [--force NAME ...] [--only NAME ...]
"""
import argparse
import os
import re
import sys

from PIL import Image

sys.path.insert( 0, os.path.dirname( os.path.abspath( __file__ ) ) )
import make_textures  # noqa: E402  (generate(), the paths)

# Small pictures: the quicker model first, one place (the larger one is often busy and slow).
make_textures.MODELS = ["gemini-3.1-flash-image", "gemini-3-pro-image"]
make_textures.LOCATIONS = ["global"]
make_textures.IMAGE_SIZE = "1K"
ROOT = make_textures.ROOT
OUT = os.path.join( make_textures.OUT, "items" )
RAWDIR = os.path.join( make_textures.RAW, "items" )

STYLE = ( "Inventory item illustration for a dark science fiction action role-playing game, a single object centred and "
          "fully visible on a plain near-black background, three-quarter view, dramatic rim light, painterly realism "
          "with visible brush strokes, grounded worn industrial technology, no text, no letters, no logos, no frame, "
          "no hands, no people" )

# What a kind looks like (by its family; the tier adds polish).
FAMILY = {
    "rifle": "a long magnetic gauss rifle with a slim barrel, an amber status strip and a worn gunmetal body",
    "carbine": "a compact bolt carbine with a short barrel and a folding stock, scuffed",
    "scatter": "a heavy scattergun with a wide flared muzzle and a drum magazine",
    "rail": "a very long rail gun with copper magnetic coils along the barrel",
    "plasma": "a bulky plasma thrower with a glowing orange chamber",
    "arc": "an arc projector with a forked emitter tip crackling with blue lightning",
    "hammer": "a mining ram hammer with a long haft, a heavy head and glowing blast bolt chambers",
    "axe": "a heavy splitting axe with a serrated mining blade",
    "pick": "a long mining pick with a hardened steel spike",
    "maul": "a shock maul, a heavy club with electric coils around its head",
    "breaker": "a bolt breaker hammer with a piston-driven head",
    "drill": "a drill lance: a long shaft ending in a spiral drill bit",
    "swarm": "a cloud of small flat disc drones with cyan lights around a wrist controller lying on its own",
    "needle": "a cluster of long slender needle-shaped drones with white lights",
    "spark": "small drones with crackling electric arcs jumping between them",
    "ember": "heavy drones with glowing orange incendiary cores",
    "acid": "drones dripping a green corrosive fluid",
    "guard": "drones carrying small hexagonal shield plates linked by blue light",
    "blades": "a small survey drone's two folded cutting blades on short arms",
    "vest": "a padded armoured vest",
    "plate": "a plated combat suit torso",
    "exo": "a powered exoskeleton torso frame with servos",
    "helm": "a sealed hard-shell space helmet",
    "visor": "a helmet with a wide amber targeting visor",
    "rebreather": "a helmet with a rebreather mask and filter canisters",
    "gloves": "a pair of armoured work gloves",
    "grips": "a pair of tactical gloves with servo finger plates",
    "shockgloves": "a pair of gauntlets with electric coils on the knuckles",
    "boots": "a pair of heavy armoured boots",
    "magboots": "a pair of magnetic boots with glowing sole plates",
    "treads": "a pair of insulated hazard boots with thick treads",
    "implant_reflex": "a small neural implant chip with fine filaments",
    "implant_cardio": "a cardiac implant like a tiny mechanical heart",
    "implant_cooler": "a cooling implant with tiny metal fins",
    "implant_scav": "an ocular implant lens with a sensor ring",
    "implant_adren": "an adrenal pump implant with a small red vial",
    "implant_stim": "a stim dispenser implant with a needle port",
    "implant_focus": "a targeting eye implant with nested lenses",
    "implant_leech": "a blood filter implant with dark red tubes",
    "implant_ward": "a subdermal armour mesh implant",
    "implant_lungs": "a lung implant with small mechanical bellows",
    "implant_filter": "a radiation filter implant with lead layers",
    "implant_hunter": "a machine reader implant with a data jack",
    "implant_duelist": "a reflex booster implant glowing red",
    "implant_reaper": "a vital sign pump implant pulsing red",
    "implant_area": "a field amplifier implant with a small antenna",
    "medbelt": "a utility harness with stim injector pouches",
    "salvagebelt": "a salvage harness with tools and scrap bags",
    "airbelt": "a harness with two air tanks and hoses",
    "emitter": "a personal shield emitter, a disc generator projecting a faint blue bubble",
    "quickemitter": "a compact fast-charging shield emitter with bright cyan capacitors",
    "bulwark": "a large heavy shield emitter with thick coils",
    "fusioncell": "a cylindrical fusion power cell glowing white-blue",
    "plasmacell": "a power cell with swirling orange plasma inside",
    "crystalcell": "a power cell holding a glowing violet crystal",
    "mod_damage": "a small square circuit module with a red glowing core",
    "mod_crit": "a small square circuit module with a yellow lens",
    "mod_rate": "a small square circuit module with green coils",
    "mod_burn": "a small square circuit module with an orange flame-like chip",
    "mod_shock": "a small square circuit module with a blue spark chip",
    "mod_health": "a small square circuit module with a pink pulsing light",
    "mod_armor": "a small square circuit module with grey plating",
    "mod_shield": "a small square circuit module with a cyan disc",
}
TIER = { 1: "", 2: ", a military version with cleaner lines and darker metal",
         3: ", an elite version, sleek, with faint glowing accents and precious metal inlays" }


def parse():
    """The kinds, uniques and set pieces from Items.cpp, the English names from the text files."""
    src = open( os.path.join( ROOT, "src", "Items.cpp" ), encoding="utf-8" ).read()
    kinds = re.findall( r'\{ "item\.([a-z0-9.]+)"', src )
    en = {}
    for f in os.listdir( os.path.join( ROOT, "assets", "text", "en" ) ):
        for line in open( os.path.join( ROOT, "assets", "text", "en", f ), encoding="utf-8" ):
            m = re.match( r"^([A-Za-z0-9_.]+)\s*=\s*(.*)$", line.rstrip( "\n" ) )
            if m:
                en[m.group( 1 )] = m.group( 2 )
    # The uniques' kinds (by index or by name), the set pieces' kinds.
    block = src[src.index( "const std::vector<Unique> Uniques" ):src.index( "/** @brief The sets" )]
    uniques = []
    for m in re.finditer( r'^\s*\{ (?:(\d+)|kind\( "item\.([a-z0-9.]+)" \)), \d+,', block, re.M ):
        uniques.append( kinds[int( m.group( 1 ) )] if m.group( 1 ) else m.group( 2 ) )
    block = src[src.index( "const std::vector<SetItem> SetItems" ):]
    pieces = re.findall( r'\{ (\d+), kind\( "item\.([a-z0-9.]+)" \)', block )
    return kinds, uniques, pieces, en


def describe( kind ):
    tier = 3 if kind.endswith( "3" ) else 2 if kind.endswith( "2" ) else 1
    base = re.sub( r"\d$", "", kind ).replace( ".", "_" )
    return FAMILY.get( base, FAMILY.get( base.split( "_" )[0], "a piece of equipment" ) ) + TIER[tier]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument( "--force", nargs="*", default=[] )
    ap.add_argument( "--only", nargs="*", default=None )
    ap.add_argument( "--reverse", action="store_true", help="from the last (a second run alongside the first)" )
    ap.add_argument( "--part", default=None, help="k/n: only every n-th picture from the k-th (runs side by side)" )
    ap.add_argument( "--local", action="store_true", help="paint the missing ones locally (FLUX.2), not on Vertex" )
    args = ap.parse_args()
    os.makedirs( OUT, exist_ok=True )
    os.makedirs( RAWDIR, exist_ok=True )
    kinds, uniques, pieces, en = parse()
    jobs = []
    for k in kinds:
        jobs.append( ( "item_" + k.replace( ".", "_" ), describe( k ) + ". " + STYLE ) )
    for n, k in enumerate( uniques, 1 ):
        name = en.get( "unique.%d" % n, "" )
        lore = en.get( "unique.%d.about" % n, "" )
        jobs.append( ( "unique_%d" % n, describe( k ) + ", a legendary one-of-a-kind piece called \"%s\" (%s), "
                       "ornate details, a warm orange glow, battle-worn" % ( name, lore ) + ". " + STYLE ) )
    sets = {}
    for i, ( s, k ) in enumerate( pieces, 1 ):
        setname = en.get( "set.%d" % ( int( s ) + 1 ), "" )
        jobs.append( ( "set_%d" % i, describe( k ) + ", part of a matching equipment set called \"%s\", a shared green "
                       "accent colour and emblem" % setname + ". " + STYLE ) )
    if args.part:
        k, n = ( int( v ) for v in args.part.split( "/" ) )
        jobs = jobs[k::n]
    if args.local:
        # All missing pictures in one local batch first; the loop below then only scales them.
        for name in args.force:
            raw = os.path.join( RAWDIR, name + ".jpg" )
            if os.path.exists( raw ):
                os.remove( raw )
        todo = [( os.path.join( RAWDIR, name + ".jpg" ), "1:1", prompt ) for name, prompt in jobs
                if args.only is None or name in args.only]
        print( "painted locally: %d" % make_textures.paint_local( todo, "items" ), flush=True )
    for name, prompt in ( reversed( jobs ) if args.reverse else jobs ):
        if args.only is not None and name not in args.only:
            continue
        raw = os.path.join( RAWDIR, name + ".jpg" )
        out = os.path.join( OUT, name + ".png" )
        if not args.local and ( not os.path.exists( raw ) or name in args.force ):
            try:
                make_textures.generate( "items/" + name, "1:1", prompt )
            except Exception as e:  # noqa: BLE001  (one failure must not stop the rest)
                print( f"{name}: failed ({e})" )
                continue
        if not os.path.exists( raw ):
            print( f"{name}: not painted" )
            continue
        if not os.path.exists( out ) or name in args.force:
            Image.open( raw ).convert( "RGB" ).resize( ( 384, 384 ), Image.LANCZOS ).save( out )
        print( f"{name}: done", flush=True )
    return 0


if __name__ == "__main__":
    sys.exit( main() )
