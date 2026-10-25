# Production plan

The game follows the user's story outline (*DARK CONCORDANCE* / *KATABASIS* – title open): a prologue, four acts,
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
| Voice | local TTS in batches: F5-TTS German (the user's choice in Text2Video), Qwen3-TTS as alternative; for a sale Chatterbox (MIT) or Qwen3-TTS (Apache) instead of F5 (non-commercial weights). Lines live in the level files with ids; `assets/voice/<id>.ogg` is played when present |
| Music | renders from the VRAudio generators (Noctuary for Velm, Empyrean for the Concordance, Totality/Skybreak for fights) |
| Sound effects | synthesised in code for now |

## Systems

Built with level 1: grid levels, low gravity with jumps and recoil, ranged combat with auto-aim and mouse/right-stick
aim, flow-field pathing, enemy state machines, boss with weak points, rescue capsules as checkpoints, the hidden death
counter, pickups (scrap, stims), subtitles and hints, HUD, wall cut-away around the hero, synthesised sound; after the
user's first play (25.10.2026): zones with themes, signs, solid props, a freight lift as the end, difficulty levels
(Leicht with slow regeneration), brightness, title and pause menus, the boss's weak points marked and announced.

Still to come, by first use:

| System | First needed in |
|---|---|
| Tilting gravity (thrust pulses) | Prologue |
| Melee with combos and shield (Dorra) | Prologue / class choice |
| Rotating drum: gravity falls towards the axis, Coriolis bends shots, aim-ahead mode | 2 Knochenmarkt |
| Vacuum: oxygen, helmet hits, radiation spikes, only radio and breath | 3 Außenhaut |
| Floating water spheres: caustics, diving, drowning, current through water | 4 Ertrunkenes Viertel |
| Fragile targets (core storage), multi-body boss with radio link | 5 Kernbank |
| Hub: bar, trader, workshop, jobs, the take-down board | Velm hub |
| Drone swarm control (Ione), hacking machines | class choice |
| Breaking ice, enemies from below | 6 Seren |
| Wind, swaying platforms, cutting balloons | 7 Hesper |
| Drift of shots in wind, heat and cold zones | 8 Halden |
| Power routing (doors against sleeper pods), choices with epilogue cards | 9 Arche |
| Zero g, freezing gear | 10 Tesh |
| Radiation as a running resource, magnetic fields | 11 Kael |
| Darkness as cover, geysers | 12 Wanderer |
| Talking to control cores instead of fighting (the night shift remembers) | 6 onwards |
| New bodies: talents redistributed once | 14 |
| Arena waves with changing rules | 15 |
| Zero-g construction site, moving girders | 16 |
| Hearing: conversation level, crowd that does not panic | 17 |
| Heat as a clock (rotating shield) | 19 |
| Terrain cut during the level | 21 |
| Jumping between containers with limited fuel | 22 |
| Molten metal, centrifuges | 23 |
| Split finale (one in the net, one guarding the bodies), three endings | 24 |
| Co-op for two (shared screen) | early, then everywhere |
| Companion flashback levels | after each act |

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

1. **Level 1 „Die Kühlgedärme“** as the playable prototype with Corvin: does the combat feel right?
2. Prologue (tutorial, Ilka, the other two classes as choices), Velm as hub, levels 2–5, voices: **Act I complete**.
3. Act II with the ship as hub and the first choices; co-op for two.
4. Acts III and IV, endings, epilogue cards, flashback levels.
5. Steam Deck: half-resolution fog, no supersampling, gamepad everywhere.

## Open decisions (the user's)

- Title: *DARK CONCORDANCE* or *KATABASIS* (collision check before it is final).
- Where the reference voices come from; voice language (German, as the story).
- Where human character models come from, if the suits are not enough.
