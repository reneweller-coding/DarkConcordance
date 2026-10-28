# Production plan

The game follows the user's story outline (*DARK CONCORDANCE*, the title since 28.10.2026): a prologue, four acts,
24 playable locations, three hubs. Combat is arcade like *Baldur's Gate: Dark Alliance*, ranged combat is aimed like
*Fallout: Brotherhood of Steel*; the story around it is not arcade. Every location gets a **wonder** (a picture one
does not forget) and a **wound** (what is done to people there), and almost every one its own mechanic.

## Look test (25.10.2026) — done

One room of an abandoned station (`--lookdev`). Findings: generated textures plus shapes built in code carry the look;
the hero made of rounded boxes reads from the game camera but looks like a toy up close; 5.7 ms a frame at 1080p x1.5
on the RTX 5090 (the Steam Deck needs half-resolution fog); the 3D generator is not used.

## How the pieces are made

| Piece | How |
|---|---|
| Levels | ASCII maps in `assets/levels/*.txt` (2 m cells: walls, floor heights, voids, cover, spawns, lights, triggers), turned into geometry and dressing by code |
| Surfaces | Gemini image model (`tools/make_textures.py`), made seamless, maps derived |
| Machines, drones, creatures | built in code from shapes, moved procedurally (IK legs, pistons, swarms) |
| People in the game | segmented suits as in the look test; for close-ups none |
| Conversations | portraits from the image model (one reference per figure, expressions from it), subtitles, voice |
| Texts | `assets/text/<language>/*.txt`, one `key = text` per line (menus and HUD in `ui.txt`, one file per level); the level files carry only the keys. German is the source and fills every gap |
| Signs | painted per language by the image model from the same text files (`make_textures.py --lang en`) |
| Voice | Qwen3-TTS locally (`tools/make_voices.py`, Apache 2.0, the user's choice after samples against VoxCPM2): a voice per figure and language designed from a description (`tools/voices.json`, kept in `tools/voices/`), then cloned for every line; radio, machine and loudspeaker effects after it. `assets/voice/<language>/<key>[.<hero>].ogg` is played when present, else the German one under the chosen subtitles; conversations speak too, the hero's answers not |
| Cutscenes | eleven films (see below), photographic, made locally: FLUX.2 pictures, LTX-2.3 motion, AV1; voice and subtitles per language at playback |
| Music | the AudioGenerator's soundtrack mode (ACE-Step 1.5 XL, local) from `tools/music.json`: a piece per place in two layers (calm, tense) on one beat grid and a fight piece, boss pieces in two layers, stingers, pieces for scenes; the game follows the danger near the hero (Score.cpp); OGG Vorbis q4 |
| Sound effects | synthesised in code; the backgrounds per place from TextureGen (Stable Audio Open), `tools/ambience.txt`, seamless loops, OGG Vorbis q3 |

## Languages

German and English are required; another language is a folder of text files, its signs and its voices, with no
change to the code.

- No visible text in the code or baked into a texture except through keys: menus, HUD, lines, hints, level names,
  sign plates.
- The language is chosen in the title and pause menus (at first the system's), kept in `settings.txt`; `--lang` for
  tests. Switching repaints the signs and reloads the font's characters.
- The font loads every character the texts use (Bahnschrift: Latin, Greek, Cyrillic; Chinese or Japanese would need
  another font).
- Per level: the German file is written with the level, the English one translated from it in the same step, then the
  signs (`make_textures.py --lang`) and the voices (one TTS batch per language).
- Control hints name keys and buttons in the text for now; glyphs per device later.

## Cutscenes

Films for the big moments (the prologue's opening, the end of each act, the three endings, the companion flashbacks'
openings); everything else plays in the engine (cheaper, the same look as the game). Made with the Text2Video tools
(`G:\Tools\Text2Video`):

1. Script per shot in `tools/films.json` (picture, motion, who says what, length), the lines in
   `assets/text/<language>/films.txt`; pictures from FLUX.2 locally (WSL2), the speakers' portraits as references.
2. Motion with LTX-2.3 distilled locally through Text2Video's production batch in WSL2 (image and silence to video on
   the RTX 5090), each shot as long as its lines need.
3. No speech in the picture: the lines are spoken by the game (the same voices as everywhere, per language) at the
   times in the film's cue list, with subtitles; under it the music cue the film names. One film serves every
   language. No lip sync: faces do not speak on screen.
4. In the game: AV1 in IVF, decoded with dav1d (BSD-2; built once for MSVC into `third_party/dav1d`), the frame
   turned into a texture (Film.cpp). AV1 rather than MPEG-1 (the first plan, pl_mpeg) or H.265 (patents, no decoder
   on Windows without a paid extension): about a quarter of MPEG-1's size, royalty-free (the user, 27.10.2026).

The films' look: photographic (the user, 27.10.2026). Built 27./28.10.2026: the prologue, the ends of Acts I-III, the
openings of Acts III and IV, the three endings, the four tales' openings; 13 films, 9 1/2 minutes (the user found the
first five minutes too little).

## Systems

Built with level 1: grid levels, low gravity with jumps and recoil, ranged combat with auto-aim (mouse aim on the PC;
the right stick turns the camera, as on the Steam Deck), flow-field pathing, enemy state machines, boss with weak points, rescue capsules as checkpoints, the hidden death
counter, pickups (scrap, stims), subtitles and hints, HUD, wall cut-away around the hero, synthesised sound; after the
user's first play (25.10.2026): zones with themes, signs, solid props, a freight lift as the end, difficulty levels
(Leicht with slow regeneration), brightness, title and pause menus, the boss's weak points marked and announced;
then a camera that turns about the hero (right stick, as on the Steam Deck) and German and English throughout.

Built with the prologue (25.10.2026): the choice of hero; Dorra's melee (three blows in a chain, the overhead slam's
blast, the shield, thrown blast bolts); boarders with batons and carbines (bolts in flight, aiming lasers); gravity from
thrust that tilts with the ship's dodges, loose cargo; people who do not fight (refugees, Ilka); a window in the floor
onto a star, the other ships' plumes and the patrol; lines per hero ("<key>.dorra"), a scripted last scene with a
painted still; levels that lead to the next; the autopilot's route through a level. The game's code lives in
Game.cpp (the level, Corvin), GameCrew.cpp (Dorra, the boarders) and GameShip.cpp (aboard a ship).

Built with the Velm hub (25.10.2026): peaceful levels; people with conversations (nodes, answers to choose, the start
node by what has happened, portraits) and events (flags, scrap, shop, workshop); the trader and the workshop (weapon
and armour in three levels each); progress carried between levels and saved (`save.txt`, „Fortsetzen“); exits that
open by event; a board painted at load time in the game's font; the view through the bar's floor onto the cavern and
the other drums turning with ours. Enemies no longer walk over chasms. The hub's code is GameHub.cpp. After the user's
look at it (25.10.2026): a street theme (house fronts, roofs, lights and washing strung across, stalls, clutter),
people who stand together or stroll, lines overheard in passing; walls on a ship lidded with plating or container roofs
instead of rock; an opening card for a level (the prologue's setup). Then: doors that open with A / E (some locked
until an event), lockers to search, terminals whose texts the inventory keeps; boarders whose suits burst apart; the
prologue rebuilt with corridors and cabins before the hold; people on stools at tables and counters; the hero
bare-headed where there is air; a map of what has been seen (over the game or as a page), an inventory and a character
page with portraits.

Built with the RPG layer (26.10.2026, after the user asked for Dark Alliance's pull of ever better gear): items in
four rarities (common, magic, rare, unique) on seven slots (weapon, armour, helmet, gloves, boots, two implants), 50
kinds in two tiers (the second from item level 7: Gauss Rifle Mk II, Spindle Rail, Forge Hammer, Combat Exo ...),
17 affixes that grow with the item's level, 18 uniques; drops from kills, champions, lockers and bosses; a bag of 30,
salvage for scrap; experience and hero levels (to 40) with a skill point each; 16 skills (five passives and three
actives per hero: Corvin's overcharge, grenade and roll, Dorra's bolts, quake and charge); hits with crits, burning,
stuns, leech and thorns (GameLoot.cpp, Items.cpp, Skills.cpp, GamePanel.cpp).

Built with real people (26.10.2026): Quaternius' Universal Animation Library 1 and 2 and the Universal Base Characters
(all CC0, downloaded with the user's OK: quaternius.com/packs/universalanimationlibrary.html,
quaternius.itch.io/universal-animation-library-2, quaternius.itch.io/universal-base-characters), GPU skinning in the
game's own shaders, the clips retargeted onto the bodies at load time; the hub's and the ship's people are skinned
figures with hair and clothes in many colours that walk, sit on stools, talk, fold their arms (Skin.cpp,
GameSkin.cpp). The heroes and the enemies stay procedural figures for now.

Built with level 2 (26.10.2026): **generated levels** (LevelGen.cpp) in the manner of Diablo II: a level file can be a
recipe (rooms on the way, side rooms, sizes, themes, enemy packs with weights, champions' share, the set piece at the
end, terminal texts); from a seed it becomes an ordinary level file: rooms joined by straight corridors (some with
doors), dressed by theme (market stalls, heat exchangers, shelves, container stacks, a shaft with a grating bridge,
raised floors), packs and champions, lockers, terminals, capsules on the way, lamps, triggers for the lines, the
autopilot's route, and a boss hall with the gate and the freight lift. The seed comes from the save, so each new game
(and each visit to the depths) has another layout; the story's set pieces stay fixed. **New enemies** (GameFoes.cpp):
market guards behind riot shields (they block from the front), heavies with rotary guns, hovering drones, saw bots in
swarms, crawlers that run in and go off; **champions** with a coloured ring, a name and one gift (swift, armoured,
volatile, berserk), a guardian at a depths level's end; **the Taxator** in his exoskeleton (stomp, fanned volleys,
help at 60 % and 30 %, a charge that ends stunned against a wall). Enemies grow with the area. The hub's lift asks
where to when it can go to more than one place.

Built with levels 3–5 (26.10.2026), all generated: **the drum** (level 2): Coriolis bends what flies (`coriolis` in
the level file; the slugs fly in bent pieces, the auto-aim and the enemies lead it), flocks of birds (`birds`).
**Vacuum** (level 3, `space`, `vacuum`, `flares`; GameWorld.cpp): open space under the hull plates with the star in
it, oxygen outdoors (huts, air posts, canisters refill; hits leak), radiation storms with warning and shelter, an edge
guard instead of falls; **the tug** (GameTug.cpp) as a boss of parts: deck gun, two engines, shells with rings, bridge
bolts, drones. **Water** (level 4, `genwater`): floating balls with caustics, diving with breath, a current, muffled
sound, damped movement and shots; shock gunners whose arc is stronger in water; **the Hydrauliker** (jet, hammer,
flooding). **Fragile targets and a boss of many bodies** (level 5, GameVault.cpp): racks of cores that break (the
auto-aim spares them, the game remembers a clean run), shells (emptied bodies), **Mother Sevenfold**: seven bodies,
three radio masts that heal and shield them while they stand. Velm's jobs chain to the end of Act I (Ilka sold to the
Mill); the lift lists every open place. Enemies and hits grow with the area (1 to 9).

Balance after the first measurements (27.10.2026): the tug sends at most two drones and Dorra's blast bolts do double
on its plating (her runs: 760 and 716 s with 1 and 0 deaths, before 908 and 1298 s with 4 and 7); Mother Sevenfold's
bodies are slower while the masts stand, stumble at each mast that falls and pause longer between blows, the masts
have less health (Corvin: 3 and 4 deaths, before 7 and 5, most of them from guards he drags into her hall).

Built with the soundtrack (27.10.2026): music that follows what happens (Score.cpp, see the README) and recorded
backgrounds per place, both made locally (tools/make_audio.py) and stored as OGG Vorbis (~50 MB together: 60 music files, 8 backgrounds).

Built with Act II (27.10.2026): the ship as hub (Unzu, Ysolde, Kiri, the trader radio, the navigation); seven levels
(6–9, 11–13) and the wreck field, all generated, each with its own themes (ICE, DECK, DUSK, PODS, STATION, SNOW, HOLD;
stone, dust and snow as ground out in nature), mechanic (GameElements.cpp: the suit's warmth, dose or temperature;
wind and gusts; ice that breaks over a glowing ocean; geysers; darkness and the hero's lamp; magnetic coils; power
doors; a guard that keeps sending squads; the act's white-out), enemies (GameFoes2.cpp: faction colours, flamers,
snipers, balloon guards, torch cutters, turrets, hounds, pump drones), bosses (the machine at the berth as the
Icebreaker, the Bell and the Plasma Lance; the exoskeleton as Okafor and the boarding leader) and choices (people who
show where the boss fell, in a fight's level); music and backgrounds made like Act I's.

Built with Tesh (27.10.2026): the ALIEN theme, wandering components (GameElements.cpp), the salvage master (the
exoskeleton's fourth man), Pell (a choice); Unzu offers the detour after the ark.

Built with Acts III and IV (27.10.2026): the Concordance – the clinic of new bodies (skills given anew once), Ring
Serenity as hub with its land rising into the sky (the camera looks up when one arrives), the Dying Gardens (rounds
with changing rules, Teodor Lamm who dies twice), the yard at the gate (evidence the cleaners wipe), the hearing (an
archon's verdict, then drones among people who don't run), the computing moon (the archon gives the codes, Unzu reads
his memories), Vault Zero (the heat shield's shadow as a clock, the archon of the Collegium as a swarm, the deaths
counted, the Lullaby, the act's end); the Mill – the ship on the way back with Ruul, Velm besieged (HAUSMEISTER's
turrets on the hero's side), Brannt being peeled (crust and cutting lines), the ore stream (thrusters, laser pulses,
the catcher's clock), the Mill's inside (molten metal, centrifuges, the foreman), the control room (Ysolde), the net
(the night shift fights along; Ilka; three endings, the third only if the night shift trusts the hero) and the
epilogue's cards. New surfaces for the later places of every act and portraits for every speaker; music and
backgrounds for both acts. Seren's and Kael's control cores now speak after their fights (the night shift remembers).

Ione (27.10.2026): the third hero with her drone cloud – a cutting swarm, a screen that tires it, the takeover of
machines, first aid and the cutting storm; her own swarms, skills and lines (Kael above all). The companions sit in
every hub and talk through each act; four tales are played as their tellers (Corvin's confession on Odile, Ione's
incident report on Kael, Dorra's day forty-one on Kalvar, Unzu's survey log of Talgrund with his drone), each in its
own light, music and surfaces; the epilogue has a card for all three heroes.

Loot (27.10.2026, the user: loot is what keeps one going in Diablo and Dark Alliance): ten slots with a harness, a
shield emitter and a power cell instead of amulets and rings, 145 kinds in three tiers, 28 affixes, 70 uniques with
powers and lore, 8 sets, sockets and modules (Kiri drills sockets), a painted picture for every item, and the gear
visible on the hero.

Dressing (27.10.2026, the user: Dark Alliance lives from its loving detail): every wall and floor texture has two
variants laid as a patchwork, about 150 decals (stains multiplied, posters, graffiti, neon and glyphs that glow),
clutter by the walls in seven kinds of place and show pieces in the larger rooms, with their lights; textures are
kept from level to level.

Enemies (27.10.2026, the user: "eigene Familien je Ort, Stufen, mehr Champion-Gaben, Resistenzen"): thirteen
families of the places' own creatures in three tiers each (13 levels, three guardians), four kinds of damage with
resistances and weaknesses for every enemy, twelve champion gifts (two from Act III on), a leader with an escort in
every generated level, burning, corroding and freezing ground, the hero chilled, burning or corroded.

Detail round (28.10.2026, the user: as many kinds of pieces as Dark Alliance or more, all of them science fiction, no
columns): wall fittings per kind of place, wall tops with copings, pipe runs, trays, ducts and machines, things that
break (seven kinds, with pieces and drops), the finds on the floor as models; per level now about 60 kinds of wall
and top pieces, 15-35 kinds of clutter, 2-4 kinds of breakables, 14 loot models, 146 decals.

Figures and rooms (28.10.2026, the user: animated heroes, enemies and NPCs, the architecture, the balance): every
figure built of plates now rides on a skinned body moved by the animation libraries' clips (walk, run, sprint,
aim and shot, three blows, block, jump, sitting, huddling, death), its plates on the bones; the rooms lose their
square corners (cut outer corners, filled inner ones, niches) and the halls get galleries on brackets. Balance by
the autopilot's deaths: in Erzstrom Ione did not fail at the containers but at the catcher's clock (a machine of
about 26 000 points in 240 s), because the autopilot stopped 26 m from it, out of her swarm's reach (19-21 m); it now
stops where a part is within its weapon's reach. The gear's bonus against machines now counts on the bosses' machine
parts too, and every part destroyed starts the catcher's clock again. In Brannt the deaths were the column's cutting
lines and the falls through the crust they crack, and a trap: a capsule or the last safe spot on cut crust dropped
the hero back in every eight seconds; cut crust now closes at once where the hero is put back. The column cuts less
often (every 3.6 s and 5 s instead of 3 s and 4.4 s), a cut costs 24 % of the health instead of 28 %.

Round of the user's wishes (28.10.2026, "bitte baue das alles ein"): buttons drawn as the device in hand (Deck,
Xbox, PlayStation, keys), hit reactions, a heavy blow's knockback, a dodge roll for every hero, the left hand on the
rifle, feet on steps; New Game+ and Albtraum; a codex and 42 awards across all games; walkways with a shooter and
railings in the larger rooms; saving at rescue capsules; an options menu with the subtitles' size, colour vision and
the camera's shake; Barlow as the font where Bahnschrift is missing (Proton, Linux).

Open: the user's play (balance, the music's choices and levels). Co-op is set aside for now (the user, 27.10.2026).

Still to come, by first use:

| System | First needed in |
|---|---|
| Rotating drum: Coriolis bends shots, aim-ahead | 2 Knochenmarkt — done (26.10.2026) |
| Vacuum: oxygen, helmet hits, radiation spikes, only radio and breath | 3 Außenhaut — done (26.10.2026) |
| Floating water spheres: caustics, diving, drowning, current through water | 4 Ertrunkenes Viertel — done (26.10.2026) |
| Fragile targets (core storage), multi-body boss with radio link | 5 Kernbank — done (26.10.2026) |
| Drone swarm control (Ione), hacking machines | class choice — done (27.10.2026) |
| Breaking ice, enemies from below | 6 Seren — done (27.10.2026) |
| Wind, swaying platforms, cutting balloons | 7 Hesper — done (27.10.2026: wind, gusts, balloon guards; the platforms do not sway) |
| Drift of shots in wind, heat and cold zones | 8 Halden — done (27.10.2026) |
| Power routing (doors against sleeper pods), choices with epilogue cards | 9 Arche — done (27.10.2026; the epilogue cards still to come) |
| Zero g, freezing gear | 10 Tesh — done as low gravity, cold everywhere and wandering components (27.10.2026) |
| Radiation as a running resource, magnetic fields | 11 Kael — done (27.10.2026) |
| Darkness as cover, geysers | 12 Wanderer — done (27.10.2026) |
| Talking to control cores instead of fighting (the night shift remembers) | 6 onwards — done (27.10.2026: six choices count) |
| New bodies: talents redistributed once | 14 — done (27.10.2026) |
| Arena waves with changing rules | 15 — done (27.10.2026) |
| Zero-g construction site, moving girders | 16 — done (27.10.2026; with evidence the cleaners wipe) |
| Hearing: conversation level, crowd that does not panic | 17 — done (27.10.2026) |
| Heat as a clock (rotating shield) | 19 — done (27.10.2026) |
| Terrain cut during the level | 21 — done (27.10.2026: the crust closes again) |
| Jumping between containers with limited fuel | 22 — done (27.10.2026) |
| Molten metal, centrifuges | 23 — done (27.10.2026) |
| Split finale (one in the net, one guarding the bodies), three endings | 24 — done as two levels and three endings (27.10.2026; co-op still to come) |
| Co-op for two (shared screen) | early, then everywhere |
| Companion flashback levels | after each act — done (27.10.2026: four tales, the companions in the hubs) |

## Locations

| # | Location | Mechanic | Enemies | Boss | Wonder |
|---|---|---|---|---|---|
| 0 | Leichter „Gute Hoffnung“ | tutorial, tilting gravity | Abschöpfer boarders | – | chain of fusion flames before a red dwarf |
| H | Station Velm (hub) | bar, trader, jobs | – | – | three rotating drums in an asteroid |
| 1 | Die Kühlgedärme | low gravity, recoil | rust lice, welder lice | Drohnenpresse | condensate swarms like a star cluster |
| 2 | Der Knochenmarkt | gravity towards the axis, Coriolis | market guards, Abschöpfer | Taxator (exoskeleton) | stalls on the opposite wall, birds that never landed |
| 3 | Die Außenhaut | vacuum, oxygen, radiation | Abschöpfer in suits | tug with deck gun | the red dwarf with crawling flares |
| 4 | Das Ertrunkene Viertel | water spheres, diving | Abschöpfer with shock guns | Hydrauliker | caustics on every wall |
| 5 | Die Kernbank | fragile storage, counter | elite, shells | Mutter Siebenfach | storage blinking like a night sky |
| H | Unzureichend Ausgestattet (hub) | ship, mess, workshop | – | – | – |
| 6 | Seren, das Glasmeer | breaking ice | pump drones (control cores) | Eisbrecher | glowing colonies under clear ice |
| 7 | Hesper | wind, platforms, balloons | guards, drones | Die Glocke | balloons over storms, standing lightning |
| 8 | Halden | wind drift, heat and cold | clearance squads | Okafor in a walker | eternal dusk, ice rain at the border |
| 9 | Arche „Langmut“ | cold, power routing | Concession boarders | boarding leader | halls of sleepers in blue light |
| 10 | Das Objekt von Tesh (optional) | zero g, freezing gear | salvage crews | Bergungsmeister | a geometry not made for humans |
| 11 | Sternheber von Kael | radiation, plasma, magnets | guards, defence drones | Plasmalanze | a river of star matter |
| 12 | Der Wanderer | darkness, cold, geysers | works security | security, then Hale (talk) | the Milky Way over nitrogen snow |
| 13 | Der Kühltransport | race through cargo | transport guard | – | the Mill close up |
| H | Ring Gelassenheit (hub) | – | – | – | land rising into the sky |
| 14 | Die Wiederverkörperung | calm, talents anew | – | – | the ring closing overhead |
| 15 | Die Sterbegärten | arena waves | champions, beasts | Teodor Lamm | waterfalls drifting with the spin |
| 16 | Die Werft am Tor | zero-g site, girders | Collegium cleaners | cleaning squad | welding sparks over 200 km |
| 17 | Die Anhörung | conversation, crowd | Collegium drones | drone swarm | 0.4 s of debate replayed for 3 h |
| 18 | Der Rechenmond | cryo zones, magnets | maintenance robots | the moon's defence | glowing cooling ribs |
| 19 | Verlies Null | heat as a clock | Collegium | Kein Grund Zur Sorge (swarm) | coronal arcs around the shield |
| 20 | Velm, letzte Woche | defence with HAUSMEISTER | landing squads, cutters | cutting platform | the Mill filling half the sky |
| 21 | Die Zerlegung von Brannt | terrain being cut | cutters (control cores) | Schnittkolonne (talk) | a moon being peeled |
| 22 | Der Erzstrom | container jumps, fuel | defence drones | Der Fänger | a still chain of containers |
| 23 | Das Innere der Mühle | molten metal, centrifuges | control cores | Der Vorarbeiter | a small sun inside a world |
| 24 | Das Leitwerk | split finale, choice | Collegium | Ysolde, then the Collegium's Archon | 40,000 people at work |

## Order

1. **Level 1 „Die Kühlgedärme“** as the playable prototype with Corvin: does the combat feel right? — done
2. **Prologue** with Dorra and Corvin to choose — done (25.10.2026); Ione — done (27.10.2026).
3. Velm as hub (bar, trader, workshop, Ysolde) — done (25.10.2026); level 2 generated, with the Depths for replay —
   done (26.10.2026); the drum's Coriolis and levels 3–5 with the job chain to the act's end — done (26.10.2026);
   still voices for **Act I complete**.
4. Act II with the ship as hub and the first choices — done (27.10.2026: the ship, levels 6–13 with Tesh as a detour,
   the wreck field; the epilogue cards still to come); co-op for two.
5. Acts III and IV, endings, epilogue cards — done (27.10.2026); the companions' tales — done (27.10.2026).
6. Steam Deck — done (28.10.2026): frustum culling for the scene and both shadow maps (the busiest levels from
   22-30 ms to 5-6 ms on the RTX 5090), three graphics steps in the menu (low on the Deck: the scene at 3/4 size,
   8 fog steps, 4 shadow samples, smaller shadow maps; instead of a separate half-resolution fog pass, since the fog is
   drawn within each surface's shader), the window at 1280x800 there, the pad's cross in the game's own menus. Not
   tried on a Deck yet; reckoned at 30-50 frames a second.
7. The second round of the user's wish list — done (28.10.2026), all but more languages and Steam: a play log for the
   play test; the player's files in their own folder, written whole or not at all; a native Linux build (WSL2, Ubuntu
   22.04, GCC 12; `-fwrapv` so both compilers lay out the same levels) and the Windows build under Wine; clothes for
   the hubs' people (shader patterns and garments on the bones, outfits by what they do); a locker shared by every
   game; a loot filter; every action's key and pad button bound freely; one to three anomalies per visit to the
   Depths with better finds; enemies that flank, wait for the others and take cover; a photo mode.
8. Lengths and the third list — done (28.10.2026): every level measured (floor, foes at the start, the route) against
   the autopilot's times; the outliers brought in (Velm in Act IV: at most eight squads; Brannt 15 rooms, 6 side ones;
   the Sterbegärten 16 and 6; Seren a little thinner; level 1 a condensate cavern and a pump gallery). Against
   *Dark Alliance*'s figures (300-600 m of way and 80-150 foes a level, 10-20 minutes, 9-12 hours) the routes
   (380-690 m) and the foes (60-130 at the start) match; the levels are longer, as wanted for ~15 hours. Then four
   saves, the display's options (window or borderless, sizes, vertical sync, frame cap, HUD size), the pad's
   vibration, a minimap, Kiri rerolling and bettering items, ten tiers of the Depths, crash reports, the play log's
   report page and the campaign test (the main path in one go). What the campaign test found: the gallery first
   beside the north-east room stalled Dorra's fight at the press (moved to the cavern); Hesper's generator read past
   the map's edge for the balloon guards on some seeds (a crash, now two cells from the edge); Velm's cutting platform
   held Corvin some 800 s and Ione over 1,200 s (its parts 55 % weaker, Ione's swarm 1.6 times on a machine's parts:
   Corvin 727 s for the whole level now, Ione still about 1,400 s). Open, the user's to decide: the heroes outgrow the places (hero
   level about the area +5 at the end of Act I, +5 to +9 later; the autopilot died once or never in 25 places), and
   scrap piles up (14,000-34,000 at the end) with little to spend it on.
9. Balance and economy — done (28.10.2026, the user's wish): the places keep up with the hero (at most three levels
   below on Leicht, one on Normal, none above), foes grow faster from the ninth area (health +10 %, harm +5 % per
   area on top; bosses, guardians, leaders and machines not), the cutting line no longer multiplied by the depth (Corvin died 25 times there at area
   28); salvaging a fifth of the worth instead of a third, the workshop to ten levels (40 to 3,000), Ottla's sealed
   crates; a finished hero carried into a new save with the difficulty chosen („Wie beginnen?“). Campaign test on
   Normal afterwards (deaths per act I/II/III/IV): Corvin 9/1/0/0, Dorra 2/0/13/1, Ione 13/0/1/0, 2.4-4.7 h of
   autopilot; scrap at the end 360-2,000 (was 14,000-34,000), the workshop at 9-10. Acts II-IV stay easier than
   act I for the autopilot; a play test should tell. Open: the autopilot (not the game) stalls now and then when a
   level ends with someone to talk to it cannot reach (Dorra in Halden and the Arche, Ione in the Erzstrom).
10. Title, autopilot, heroes, release — done (28.10.2026, the user's wish): the title *DARK CONCORDANCE*. The campaign
   test keeps the hero before each place (`--keep`, `--places`) to replay a stall alone. What it found: two faults
   of the game — waiting creatures in a corridor were written into the map as digits, which the level reads as
   floor heights (a metre-high block before the only door of the Drowned Quarter: now a "native" line, as in the
   rooms); a shipyard girder shoved the hero over an edge into space (an edge now holds like a wall: caught, not
   thrown off). And the autopilot's: the swarm's carrier out of sight (Netz), the catcher asleep beyond the route's
   end and its last arm out of sight (Erzstrom), Dorra after a foe she cannot reach (Ertrunken, Halden, Arche: she
   gives one up after eight seconds without a blow), the last waypoint near enough after fifteen seconds, the boss
   only shot at where he can be aimed at (Wanderer), centrifuges' pull avoided where there is a way (Mühle), its
   freeing step only onto floor; last resort after four minutes without progress: put on at the next waypoint
   (`STALL:`, `stalls=`). Campaign on Normal afterwards: all 75 places finished, deaths per act I/II/III/IV Corvin
   6/1/3/0, Dorra 7/0/0/0, Ione 8/5/0/0, 1.6-2.9 h of autopilot; two stalls (Corvin in the Verlies, Ione in the Mühle).
   Velm needed nothing more (Ione 212 s since the laser fix). The heroes' own bodies from MPFB (`tools/make_heroes.py`,
   `assets/models/heroes`), drawn dressed with only weapon and shield as boxes. Release: version 1.0.0,
   `Deploy/build_release.ps1` and `Deploy/build_linux.sh` (zip 1.2 GB, tar.gz with dav1d in lib/), `LICENSE`
   (AGPL-3.0), `docs/CREDITS.txt`, `docs/README-player.txt`; the log falls back to the player's folder. Open: the
   rifle's grip on Corvin's body, the other people still on Quaternius' bodies, a remote and publishing (the user's).
11. The open points — done (28.10.2026, the user's wish): the rifle's grip was a wrong T-pose (raylib reads a clip's
   first frame at time 0, and the single key lay later, so the game measured every clip from the A-pose and the arms
   crossed): the `A_TPose` now keyed at frames 0 and 1, exported from time 0. The plates brighter. Everyone else
   with a portrait, nine of the crowd and two soldiers as whole people from MPFB (27 in `assets/models/people`,
   101 MB), the everyday clothes with logos, crop tops and shorts swapped for coveralls, tunics, robes, uniforms and
   suits; shader mode 7 tints clothes per person and armour per faction. The two stalls: the planner's search for
   shade knew no "shadow" meter (every cell counted as shade, Corvin stood in the light; now under a roof or in the
   shield's sweep) and found the route's goal instead of nothing; three times stuck at one place blocks that cell
   for the planner (Ione in the Mill, 1,712 s to 671 s); a fall soon after a jump bans jumps from where it took off
   (Ione in the Rechenmond fell five times at one gap; now once), and a death counts as no progress for the last
   resort. Campaign on Normal afterwards: all 75 places finished, deaths per act I/II/III/IV Corvin 12/0/1/0, Dorra
   4/0/0/0, Ione 7/2/8/0 (the Rechenmond's falls, since learnt), 1.6-3.8 h; one stall (that gap). Packages
   rebuilt (zip and tar.gz 1.3 GB each), both started muted from the package (the Linux one in the hub).

## Open decisions (the user's)

- None at the moment.

Decided (27.10.2026, the user): voices in German and English, with Qwen3-TTS; the cutscene films photographic; co-op
not for now. Decided (28.10.2026, the user): the title *DARK CONCORDANCE* (no game of that name found); human
character models from MPFB2 (MakeHuman for Blender, CC0 assets); the code under the AGPL-3.0.
