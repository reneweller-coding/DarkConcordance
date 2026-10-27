# SpaceOpera (working title)

A dark space opera action RPG in the manner of *Baldur's Gate: Dark Alliance*: three-quarter camera, gamepad,
ranged combat aimed as in *Fallout: Brotherhood of Steel*, pools of light in the dark. The story is the user's outline
(*DARK CONCORDANCE* / *KATABASIS*); the production plan is in [docs/PLAN.md](docs/PLAN.md).

## Starting the game

„Spiel starten“ asks who you are: **Dorra Vask** (melee: ram hammer, shield, blast bolts) or **Corvin Aske**
(ranged: the Gauss rifle); Ione follows later. The game begins with the prologue, wakes up on Station Velm (the hub)
and goes down to level 1 from there, back to Velm after it; hero, scrap, stims, upgrades and what has happened go along
and are saved in `save.txt` beside the program at each level's start („Fortsetzen“ in the title menu).

## Prologue: the lighter „Gute Hoffnung“

A refugee transport braking into the Velm system, 3,000 people in cargo containers. You were paid to take nine-year-old
Ilka to her aunt on Velm. Skimmers (Abschöpfer) board the ship. An opening card says so before the first step (A or
Enter cuts it short).

- **Gravity from thrust, not spin:** the pilot dodges, and every dodge tilts gravity sideways: a klaxon, an arrow, then
  the push. Everyone slides, loose cargo slams into whoever stands in its way, the picture tilts with it.
- **Boarders:** with shock batons (they close in and swing) and carbines (they keep their distance, take aim with a red
  laser, fire bursts of slow bolts you can sidestep or catch on Dorra's shield). Beaten, their suit's power cell
  shorts out: a blue flash, sparks, smoke, and the armour plates fly apart and stay scattered on the deck.
- **The way:** the mess, where Ilka asks whether her aunt will want her; the alarm sends her running back to her
  container. Then the crew corridor with its cabins (doors, lockers, the captain's log on a terminal), the hall with
  heat exchangers for cover and loose cargo, the observation blister with a window in the floor, the bridge, the east
  corridor with its bulkhead, and only then hold four with its container stacks, the refugees and Ilka's nook.
- **Wonder:** through the window, the red dwarf and the chain of fusion flames of the other refugee ships braking
  before it. **Wound:** the Concordance patrol *Not Our Concern* answers the distress call and declines.
- **The end:** Ilka found too late, the scanner still warm, the shot from behind, the dark, and the charity clinic on
  Velm (a painted still with the doctor's lines).

![The opening card](docs/screenshots/pro_intro.jpg)
![The mess: refugees at the tables, the door to the crew corridor](docs/screenshots/pro_mess.jpg)
![A boarder's suit burst apart, the plates on the deck](docs/screenshots/pro_burst.jpg)
![Through the window in the floor](docs/screenshots/pro_view.jpg)
![Dorra against a boarder in hold four](docs/screenshots/pro_dorra.jpg)
![A dodge](docs/screenshots/pro_thrust.jpg)
![Ilka](docs/screenshots/pro_ilka.jpg)
![The clinic](docs/screenshots/pro_clinic.jpg)

## Hub: Station Velm, the Third Drum

An alley in one of the three habitat drums turning inside the hollowed asteroid. No fighting: A or E talks; there is air,
so the hero goes without a helmet.

- **The street:** house fronts with lit windows, doors with lamps, shutters, striped awnings, flat roofs with
  skylights; strings of bulbs and washing across the alley; market stalls, benches, tubs of greens, vending machines.
- **People:** standing together at the stalls and the counter, sitting by the walls, strolling about (children among
  them); passing by, one hears what they say about the Harvest, the prices, the Marrow Market (24 lines, in turn).

- **The bar „Zur Letzten Schicht“** with Ottla behind the counter (she sells stims), guests on the stools at the counter
  and at the tables (a pendant lamp over each), bottles on the shelves behind the bar, the harvest
  board on the north wall (61 settlements the Mill has taken, the newest in red, chalked in the game's own font) and a
  window in the floor: beneath it Velm turns past, the cavern's work lights, girders, the other two drums with their
  lit rooms and window bands.
- **Ysolde Varn**, who paid for the clinic: conversations with answers to choose, portraits beside the lines; she gives
  the first job (and 50 scrap down payment) and has new lines after level 1.
- **Anand's Workshop** (Kiri): the hero's weapon (more damage, less heat; Dorra's bolts come back sooner) and armour
  (+20 health) in three levels each, paid in scrap.
- **HAUSMEISTER**, the station's voice at a terminal; the clinic; the freight lift to the axis, which opens once there
  is a job to go to.

The conversations are in [assets/levels/velm.txt](assets/levels/velm.txt) (`[npcs]`, `[talk]`: nodes, answers,
events), their words in `assets/text/<language>/velm.txt`.

![The alley](docs/screenshots/hub_street.jpg)
![By the clinic, someone talking about the Mill](docs/screenshots/hub_yard.jpg)
![Ysolde Varn](docs/screenshots/hub_ysolde.jpg)
![Through the window in the bar's floor](docs/screenshots/hub_window.jpg)
![The harvest board](docs/screenshots/hub_board.jpg)
![The workshop](docs/screenshots/hub_workshop.jpg)

## Level 2 „Der Knochenmarkt“ and the Depths

The farm drum, where grain once grew; now a market for what is left of people. Ysolde gives the job after level 1
(talk to her again once she has sent for you); the lift asks where to. The level is **generated** from a recipe
([assets/levels/knochenmarkt.txt](assets/levels/knochenmarkt.txt)): twelve rooms on the way (market alleys with stalls
and cowering market folk, warehouses with container stacks, cold rooms, works), four side rooms behind doors with
lockers, a champion and terminals (the Taxator's ledger, an appraisal, a receipt with a seal), rescue capsules on the
way, and at the end the Taxator's hall behind a door: the boss in a brass exoskeleton who stomps, fires fanned volleys,
calls for help at 60 % and 30 % and finally charges. Each new game lays the market out anew; the hall stays.

**The drum turns:** the market lies on the inside of a spinning drum, so everything that flies is bent by the Coriolis
force: shots, bolts, grenades and the enemies' bolts curve sideways on long throws (the auto-aim leads them), and
flocks of birds wheel under the cavern's lights.

**The Depths** ([assets/levels/tiefen.txt](assets/levels/tiefen.txt)): the maintenance levels under the drum, open
from the hub's lift after level 1 and again and again, laid out anew on every visit, as strong as the hero, with a
guardian champion before the lift. For experience and gear.

**Enemies:** besides lice and boarders, market guards with riot shields (attack from the side), heavies with rotary
guns (a spin-up, then a long burst), hovering drones, saw bots in swarms, crawlers with charges (they go off near you
and take their friends with them). **Champions** wear a coloured ring and a name: swift (green), armoured (blue),
volatile (orange, explodes when killed), berserk (red); they always drop an item.

![A shield guard in the market](docs/screenshots/l2_guard.jpg)
![The Taxator's volley](docs/screenshots/l2_volley.jpg)
![The Taxator](docs/screenshots/l2_taxator.jpg)
![The market on the map](docs/screenshots/l2_map.jpg)
![In the farm drum, birds over the alley](docs/screenshots/l2_drum.jpg)
![The lift asks where to](docs/screenshots/hub_lift.jpg)

## Level 3 „Die Außenhaut“: vacuum, oxygen, radiation storms

The Taxator's receipt names the tug *Grace Period* at the cargo ring, outside on Velm's skin
([assets/levels/aussenhaut.txt](assets/levels/aussenhaut.txt), generated). Hull plates, gangways and catwalks over open
space with the red dwarf beneath; low gravity, no falling off the edge.

- **Oxygen:** outside, the suit holds a good minute and a half (the blue bar). The maintenance huts and the air posts
  refill it, blue canisters add some; at zero it costs health. Hits leak air.
- **Radiation storms:** the red dwarf flares every minute or so: eight seconds of warning, then six seconds of
  radiation that burns everyone in the open, enemies too. Shelter is a hut or anything tall close by.
- **The tug *Grace Period*:** a boss of parts – the deck gun on top (volleys of shells, red rings show where they
  land) and two engines at the back; bolts from the bridge (close along its hull they cannot reach), drones from its
  hangar. A broken part stays broken: without the gun no more shells. Hurt to half, it tries to cast off, sliding
  along the edge and firing faster; with all three parts gone it breaks up and its container drifts away.

![The skin over open space](docs/screenshots/l3_hull.jpg)
![The tug „Gnadenfrist“](docs/screenshots/l3_tug.jpg)

## Level 4 „Das Ertrunkene Viertel“: water

A quarter of the Second Drum where the tanks overflowed; at three metres per second squared the water gathered into
balls and stayed ([assets/levels/ertrunken.txt](assets/levels/ertrunken.txt), generated). Light dances on the floor
beneath them.

- **Under water:** slow, the shots weaker, the steps light; breath lasts about half a minute, then it costs health. A
  current pulls round in circles; the sound goes dull.
- **Shock gunners** charge where you can see it (blue sparks); in water their arc hits two and a half times as hard.
- **The Hydrauliker** (the Pumpwright), a man of steel and hoses who keeps the locks: his water jet throws you far
  back, his hammer strikes close, and he floods where you stand.

![The Hydrauliker floods the hall](docs/screenshots/l4_pumpwright.jpg)
![A street in the drowned quarter](docs/screenshots/l4_street.jpg)

## Level 5 „Die Kernbank“: Mother Sevenfold

The vaults where Mother Sevenfold stores the harvested cores until they are sold
([assets/levels/kernbank.txt](assets/levels/kernbank.txt), generated).

- **Racks of cores** like a night sky, every light someone. A hit breaks a rack; the auto-aim will not shoot through
  them, grenades, bolts and blasts will. A terminal holds the register with Ilka's name.
- **Shells (Hüllen):** emptied bodies walking on a cheap core; slow, tough, they scratch close up.
- **Mother Sevenfold:** one mind in seven bodies, linked by three radio masts. While a mast stands the bodies heal and
  shrug off hits; with the masts down they stumble and fall like anyone. Leave every rack whole and the game remembers.

![The racks](docs/screenshots/l5_racks.jpg)
![In the vaults](docs/screenshots/l5_vault.jpg)

## Act I

After each level Ysolde has the next job (the lift lists every place already open; the Depths stay open). After the
Core Bank she learns where Ilka went – sold to the Mill – and names a ship in the dock below the bank: the lift takes
the hero there, and Act II begins.

![Back on Velm after the Core Bank](docs/screenshots/hub_act1.jpg)

## Act II: the Harvest

**Hub: the »Unzureichend Ausgestattet«** ([assets/levels/unzu.txt](assets/levels/unzu.txt)), an old survey ship whose
retired archon, Unzu, the Skimmers had used as a data store: the bridge with a window in the floor (a gas giant drifts
by beneath it), the mess with Ysolde, Kiri's workshop, the trader radio (stims), the hold with the airlock. Ysolde
follows the cold chain of the stolen cores from place to place; Unzu comments each one with figures from his old
survey and slowly notices that memories are missing. The airlock's navigation lists every open destination, and the
**wreck field** (generated anew each time, as strong as the hero) stays open for practice and loot.

Every place has its own look (new themes: ice, refinery deck, twilight rock, sleeper halls, star lifter station, snow,
transport hold; out in nature the ground is stone, dust or snow), its own mechanic, enemies, boss, music and background:

| Level | Mechanic | Enemies | Boss |
|---|---|---|---|
| 6 Seren, the Glass Sea | floes of clear ice over a glowing ocean; ice breaks under blasts and Dorra's hammer, a fall through costs health and warmth; the suit's warmth fades in the open (huts, heater posts) | Concession guards in white and orange, snipers, torch cutters, pump drones rising out of bore holes | the Icebreaker, a drilling rig in the open water whose control core thinks the fight is another shift |
| 7 Hesper, the refinery in the clouds | platforms over a thousand kilometres of storm; wind bends what flies, gusts sweep enemies off the edges; lightning, balloons | balloon guards hanging over the abyss (shoot the balloon), snipers, drones | the Bell, a guard airship; then a control core asks to be switched off (a choice) |
| 8 Halden, the twilight world | heat on the day side, frost on the night side, a twilight band between; a hot wind bends shots | clearance squads in olive: flamers, hounds | Clearance leader Okafor in a combat walker; beaten, she offers a deal (a choice) |
| 9 The ark »Langmut« | cold everywhere, little light; every door needs power – route it from the sleepers' pods (it costs sleepers) or fit a power cell from a side room | Concession boarders with torch cutters | the boarding leader in a cutting exoskeleton; then the ship's administration asks what to tell 80,000 sleepers (a choice) |
| 10 The object of Tesh (optional) | an alien structure; hardly any gravity, cold everywhere, wandering components that crush | salvage crews | the salvage master in his cutting scaffold; then Pell, the last researcher (a choice) |
| 11 The star lifter of Kael | a river of star matter across the sky; the radiation dose builds up in the open, plasma gusts, magnetic coils that bend shots | turrets, snipers, cutters | the Plasma Lance, whose control core was installed by Ione |
| 12 The Wanderer | total darkness (enemies see the hero only up close unless he stands in a mast's light; he carries a lamp), cold, nitrogen geysers | works security in black, snipers, turrets | the works security's guardian; then Director Hale talks (a choice) |
| 13 The cold-chain transport | the guard keeps sending squads, the clock to the Mill runs; cargo windows show the Mill | the transport guard | the guard leader – then two ships of the Silent Collegium, and everything goes white: the end of Act II |

The choices (balloon 17, Okafor, the ark, Hale) are kept as flags for the epilogue. The level numbers follow the
story's outline; level 10, the object of Tesh, is an optional detour Unzu offers after the ark: an alien structure in
the comet cloud, hardly any gravity, cold everywhere, wandering components that crush, the salvage master in his
cutting scaffold, and Pell, the last researcher (a choice). Someone who waits to be spoken to in a
fight's level (`npcafter id event` in the level file) shows where the boss fell once it has happened.

![Aboard the »Unzureichend Ausgestattet«: Ysolde in the mess](docs/screenshots/hub_ship.jpg)
![Seren: floes of clear ice over the ocean](docs/screenshots/l6_seren.jpg)
![Hesper: a refinery deck over the storm](docs/screenshots/l7_hesper.jpg)
![Halden: a settlement in the twilight](docs/screenshots/l8_halden.jpg)
![The ark: a door without power](docs/screenshots/l9_arche.jpg)
![Tesh: a wandering component in the object's halls](docs/screenshots/l10_tesh.jpg)
![Kael: the Plasma Lance](docs/screenshots/l11_kael.jpg)
![The Wanderer: the hero's lamp in the dark](docs/screenshots/l12_wanderer.jpg)
![The cold-chain transport: crates of cores, awake](docs/screenshots/l13_transport.jpg)

## Act III: the Concordance

The transport goes white, and the heroes wake in new bodies: **14 The Reembodiment**
([assets/levels/klinik.txt](assets/levels/klinik.txt)) is a quiet clinic on Ring Serenity – a mirror, Ysolde on the
terrace, and the body counselling that frees every skill point once to be given anew. Arriving there (and on the
ring), the camera first looks up: the land rises on both sides into the sky and closes overhead into a thin arch, its
far side in the night, its towns glittering (`view ring`, `vista s`; the ring is a band drawn by the planet shader in
its ring mode, its land painted at load). **Hub: Ring Serenity** ([assets/levels/gelassenheit.txt](assets/levels/gelassenheit.txt)):
Ysolde's flat, the garden with Kiri, a dispenser on the promenade, Unzu on a terminal from the dock, a tram to the
destinations.

| Level | Mechanic | Enemies | Boss |
|---|---|---|---|
| 15 The Dying Gardens | gardens where citizens die for fun; in the arena four rounds with changing rules (low gravity, darkness, a storm off the ring wall, quick champions only); waterfalls drifting with the spin | duellists and arena gunners in teal, bred predators | Teodor Lamm in his arena frame – he dies once and is printed again |
| 16 The Yard at the Gate | scaffolding over the stars, hardly any gravity, no air, girders that wander and crush; freight papers on terminals that the cleaners wipe one after another (`evidence`) | the Silent Collegium's cleaners in grey, snipers, torch cutters | the cleaners' chief in his frame and his squad |
| 17 The Hearing | first a conversation with an archon's avatar (the papers saved decide its words), then the attack in halls full of people who don't run (`crowd`, `pressureafter`) | the Collegium's drones | a swarm of drones fed by three carriers |
| 18 The Computing Moon | cold everywhere, superconducting coils that bend shots; the archon's voice from every wall | saw bots, maintenance robots, turrets | the moon's defence: maintenance robots steered by computing cores; then the archon gives the codes, and Unzu reads his memories |
| 19 Vault Zero | the heat shield turns: outside, whoever leaves its shadow burns (`meter shadow`, `shadow`) | cleaners, snipers | No Cause For Concern, a swarm of thousands that dives all at once; then Ruul, then Ysolde – the deaths counted since the prologue, the Lullaby, and everything goes white |

## Act IV: the Mill

**Hub: the ship on the way back** ([assets/levels/unzu4.txt](assets/levels/unzu4.txt)): Ruul in the mess instead of
Ysolde, Unzu with his memories, Kiri, the trader radio; the wreck field and the depths stay open.

| Level | Mechanic | Enemies | Boss |
|---|---|---|---|
| 20 Velm, the Last Week | the hub of Act I as a battlefield, the Mill filling the windows; landing squads keep coming; HAUSMEISTER's turrets fight on the hero's side (`thing ally`) | Concession guards, torch cutters, saw bots | the cutting platform at the hull: lines that glow, then cut |
| 21 The Dismantling of Brannt | a moon being peeled: crust over glowing rock, cutting lines that slice it away (it closes again after a while; `crust`, `cuts`), low gravity, no air | cutters, drones, Concession guards | the cutting column – one of its cutters hesitated, and talks afterwards (a choice: the 300 refusers in the old mine) |
| 22 The Ore Stream | container roofs over the stars; thrusters for jumps in mid-air (`jetfuel`), the Mill's laser pulses (`pulses`) | guard drones, turrets | the catcher – before its clock runs out (`clock`) |
| 23 Inside the Mill | channels of molten metal (`lava`), centrifuges that fling one outward (`thing spin`) | machines with control cores | the foreman in his crane walker; then he talks (a choice) |
| 24 The Control Room | server halls, the Collegium's squads | cleaners | Ysolde in her frame |
| 25 The Net | the night shift's world, patched from canteens, classrooms and kitchens; people who stay at their places; the night shift's flashes take part of the swarm (`zaps`) | deletions | No Cause For Concern's deletions; then Ilka, and the ending |

**The endings.** Ilka offers three: the Lullaby (the forty thousand go out, Velm stays), the Disclosure (everything
sent to every station), the Night Shift (the codes to Ilka) – the last only if the night shift trusts the hero: three
of six ways a control core was treated as someone (the Core Bank's racks spared, the Icebreaker's core taken along,
balloon 17 switched off as it asked, the Lance's message sent, the hesitant platform asked to cut around the mine, the
foreman spared). An answer may need a flag (`answer>node?flag`, `?!flag` its opposite). Then the **epilogue**: a card
for the ending, one for each great choice made on the way (the Core Bank, Seren, Hesper, Okafor, the ark, Pell, Kael,
Hale, Brannt, the foreman), the hero, Unzu (who asks for a new name), Ysolde and the harvest board (`epilogue event`
in the level file; GameStory.cpp). The deaths are counted across the game; Ysolde names the number at the end of Act
III (`{deaths}` in a text), and from then on every respawn shows the Collegium's mark.

**New in the code:** GameAct34.cpp (the ring, the vista, rounds, evidence, the shadow, crust and cutting lines, pulses,
jets, centrifuges, molten metal, allied turrets, the catcher's clock, waterfalls, crowds), GameSwarm.cpp (a boss of
many: carriers that make units, units that circle and dive, the archon's dive all at once, the night shift's flashes),
GameStory.cpp (deaths, trust, the act's end card, the endings, the epilogue). The exoskeleton has four more men
(Lamm, the cleaners' chief, the foreman, Ysolde), the berth three more machines (the cutting platform, the column, the
catcher). Things without a map character are placed by name (`thing r c name`, generated by `genthing name rate
where`). The factions' men are called by their own names (the Concession's guards, the Collegium's cleaners).

![Arriving on Ring Serenity: the land rises into the sky](docs/screenshots/hub_ring.jpg)
![The Dying Gardens: sandstone, sand and hedges](docs/screenshots/l15_gardens.jpg)
![The yard at the gate: scaffolding over the stars](docs/screenshots/l16_yard.jpg)
![The hearing: halls of polished stone](docs/screenshots/l17_hearing.jpg)
![The computing moon: the archon speaks](docs/screenshots/l18_moon.jpg)
![Vault Zero: a gallery outside, the star's shadow bar](docs/screenshots/l19_vault.jpg)
![Velm, the last week: the alleys under siege](docs/screenshots/l20_velm.jpg)
![Brannt: a cutting line glows on the crust](docs/screenshots/l21_brannt.jpg)
![The ore stream: containers over the stars](docs/screenshots/l22_stream.jpg)
![Inside the Mill: plating, slag and racks](docs/screenshots/l23_mill.jpg)
![The control room: server racks](docs/screenshots/l24_control.jpg)
![The net: the night shift's world](docs/screenshots/l25_net.jpg)
![An ending's card](docs/screenshots/epilogue.jpg)

## Items, levels, skills

Kills, champions, lockers and bosses drop gear with a beam and a label: common, magic, rare and unique, for seven
slots (weapon, armour, helmet, gloves, boots, two implants), 50 kinds in two tiers, 17 affixes (damage, rate, crits,
burning, shock, leech, health, armour, regeneration, speed, cooling, cooldowns, finding, experience, thorns, stims) that
grow with the item's level, 18 uniques. Walking over an item picks it up (and puts it on if the slot is empty); the
inventory compares, puts on, takes off and salvages for scrap. Experience gives hero levels (to 40) and a skill point
each: per hero five passives and three active skills (Corvin: overcharge B/F, grenade R/LB, roll G/RB; Dorra: blast
bolts B/F, quake R/LB, charge G/RB).

## Map, inventory, character

![Inventory](docs/screenshots/ui_inventory.jpg)
![Character](docs/screenshots/ui_character.jpg)
![Map](docs/screenshots/ui_map.jpg)

## Dorra

Short and broad from a world of 1.9 g, in the plates of her old mining suit. The trigger strikes; held, the blows run
forehand, backhand and the overhead slam, whose blast bolt goes off in a ring. The left trigger (Shift) raises the
shield: shots and blows from the front are stopped, a boarder's baton is thrown back. B (F) throws a blast bolt in an
arc at what the auto-aim picks: for gunners far off, and for the drone press's core, which hangs too high for the
hammer; its tanks she smashes at the base. 140 health, a little slower than Corvin.

## Level 1 „Die Kühlgedärme“

Heat-exchanger tunnels near Velm's axis: little gravity, fog, rust-red emergency light. Corvin Aske with his Gauss rifle
against rust lice and welder lice, across a cooling shaft, past the cold store with the empty bodies, to the drone
press that builds new lice from scrap.

- **Low gravity:** long jumps; every shot pushes Corvin back, on the ground a little, in the air a lot.
- **Aiming:** the rifle takes the nearest target ahead (as in Brotherhood of Steel); on the gamepad that is all, since
  the right stick turns the camera (as on the Steam Deck); on the PC the mouse can aim by hand, with a little assist.
  Holding the left trigger or Shift keeps Corvin's facing, so he can back off firing. The rifle heats up.
- **Camera:** turns about Corvin with the right stick or by dragging with the right mouse button, the stick's up and
  down zoom; moving is always relative to the screen; a stick click or Backspace turns it back.
- **Enemies:** rust lice (six legs in a tripod gait, they crouch and leap), welder lice (keep their distance, spit
  molten metal in arcs); they find their way on a flow field.
- **The drone press:** a ram that slams on its anvil (klaxon first, the anvil glows red), hatches that spit new lice,
  two coolant tanks as weak points (marked on screen, the boss bar in tank / core / tank); with both burst, the
  shutters open on the core. Corvin and the hints say what to do; shots on the armour bounce off grey.
- **Areas of their own:** the maintenance access (warm light, crates), the heat exchanger hall (cold blue, many pipes,
  puddles), the shaft (raw rock, rust, cable drums), the cold store (pale frosted panels), the drone works (hazard
  paint, barrels, scrap); signs name them. The level ends on a freight lift down to the Knochenmarkt.
- **Rescue capsules** are checkpoints (and count the deaths, quietly); scrap and stims to pick up; spoken lines as
  subtitles (voices from `assets/voice/<id>.ogg` once they exist); hints; walls dissolve where they hide Corvin.
- **Difficulty** Leicht (less damage, weaker enemies, health returns slowly), Normal, Schwer; **brightness** in the
  menu; both kept in `settings.txt` beside the program.
- **Sound** is synthesised at start (rifle, hits, lice, press, steam, hum); `--mute` silences it.
- **Languages:** German and English (menu, at first the system's language). Every text lives in
  `assets/text/<language>/` (`key = text`), German fills the gaps; the signs are painted per language, voices go to
  `assets/voice/<language>/<key>.ogg`.

Move with WASD, the arrows or the left stick; turn the camera with the right stick or the right mouse button held;
jump with Space or A; shoot (Dorra: strike) with the left mouse button, the right trigger or X; hold the facing
(Dorra: the shield) with the left trigger or Shift; B or F throws Dorra's blast bolt; Q or Y injects a stim; the mouse
wheel, the shoulder buttons or the right stick's up and down zoom; Esc or Start pauses (difficulty, brightness,
language, restart, quit); F1 frame rate, F12 picture. E (or A, where something is within reach) opens doors, searches
lockers, reads terminals and talks; M, Tab or the pad's up shows the map of what the hero has seen over the game; I or
C (the pad's View button) open the inventory (gear, supplies, jobs, notes read) and the character page (portrait,
health, damage, upgrades), with the full map as a third page.

The level is a text file, [assets/levels/kuehlgedaerme.txt](assets/levels/kuehlgedaerme.txt): an ASCII map of 2 m
cells (legend at its top), zones with their themes, signs, the keys of the spoken lines and the hints with their
triggers; their words are in [assets/text/de/kuehlgedaerme.txt](assets/text/de/kuehlgedaerme.txt) and
[assets/text/en/kuehlgedaerme.txt](assets/text/en/kuehlgedaerme.txt).

![The shaft](docs/screenshots/l1_jump.jpg)
![The condensate swarm](docs/screenshots/l1_drops.jpg)
![The drone press](docs/screenshots/l1_press.jpg)

## Look test (`--lookdev`)

The first room: an observation deck with a glass floor over a gas giant and a nebula, sun shafts, a monolith.

![Monolith and the glass floor](docs/screenshots/final_a.jpg)

## Build and run

    .\build.ps1          # msvc Release; the game lands in bin\msvc\SpaceOpera.exe

Tests and pictures without a window (always silent):

    bin\msvc\SpaceOpera.exe --shot work\a.png --autoplay --report --warm 600 --hero dorra     # the prologue
    bin\msvc\SpaceOpera.exe --shot work\a.png --level kuehlgedaerme.txt --autoplay --report --warm 900
    bin\msvc\SpaceOpera.exe --shot work\a.png --level velm.txt --autoplay --report --warm 120              # the hub
    bin\msvc\SpaceOpera.exe --shot work\d.png --level velm.txt --flags job1,done.kuehlgedaerme --talk ysolde
    bin\msvc\SpaceOpera.exe --shot work\b.png --level kuehlgedaerme.txt --at 25,25 --face 180 --awake --fire --warm 4
    bin\msvc\SpaceOpera.exe --lookdev --shot work\c.png --at 0.6,-4.3 --face 145 --walk
    bin\msvc\SpaceOpera.exe --shot work\k.png --level knochenmarkt.txt --seed 3 --herolevel 5 --loot 12 --autoplay --report --warm 1500
    bin\msvc\SpaceOpera.exe --shot work\t.png --level tiefen.txt --seed 1 --dumpgen work\tiefen_1.txt --warm 1

`--report` writes the run (finished, time, deaths, kills, shots) into `spaceopera.log` beside the program; `--bench n`
renders n frames first and logs the time per frame. `--autoplay` without `--shot` lets the autopilot play in the window. `--difficulty leicht|normal|schwer` overrides the saved setting; `--menu` puts the title menu over a `--shot` picture;
`--camyaw deg` turns the camera, `--move x,y` holds the left stick during the warm-up; `--lang de|en` picks the
language; `--hero dorra|corvin` skips the choice; `--level FILE` starts elsewhere than the prologue; `--flags a,b`
sets what has happened, `--talk id` opens a conversation and `--shop 1|2` the trader or the workshop for the picture;
`--panel 0|1|2|3` shows the inventory, skills, character or map page, `--map` the map over the game; `--seed n` picks
a generated level's layout, `--dumpgen FILE` writes the generated level file, `--herolevel n`, `--loot n` (items
scattered round the hero) and `--xp n` set up a hero for tests, `--trace s` logs the run (and the music's piece and
heat) every s seconds. `--silent` runs the sound (device, music, backgrounds) at volume 0, also with `--shot`;
`--scoretest FILE` mixes a scripted stretch of music (title, a level calm to fight, a boss, the stingers, a change of
takes) into a WAV without playing it; `--dumpsound DIR` writes the synthesised effects and backgrounds as WAVs.

## Music and sound

**The music follows what happens.** Every place has its own piece in two layers that play in lockstep: calm while
nobody is near, its busier layer fading in once an enemy or two are close and awake. With three or more (a champion
counts two, or the hero badly hurt) the place's fight piece takes over, and five quiet seconds after the last enemy
the calm piece comes back. Boss fights have their own piece (Mother Sevenfold hers) whose second layer comes in for
the boss's second half: the press's tanks gone, the tug casting off, the masts down. Pieces cross-fade; a stinger
marks a boss beaten or the hero down; the prologue's end and the end of Act I have pieces of their own; the title
menu has the main theme. Each piece has two takes, which take turns every second time round. A level's pieces are
decoded when it loads, so a fight's music is there when the fight begins. „Musik“ in the menus sets the volume
(`settings.txt`).

| Piece | Where | Layers |
|---|---|---|
| title | title menu, choice of hero | – |
| prologue, prologue_fight | „Gute Hoffnung“ | calm, tense; fight |
| velm | Station Velm | – |
| kuehlgedaerme, knochenmarkt, tiefen, aussenhaut, ertrunken, kernbank (each with `_fight`) | the levels | calm, tense; fight |
| boss | the press, the Taxator, the tug, the Hydrauliker, guardians | fight, rage |
| mother | Mother Sevenfold | fight, rage |
| victory, death | stingers | – |
| loss, actend | the prologue's last scene, the end of Act I | – |
| ship, ring | the ship (Acts II and IV), Ring Serenity | – |
| seren, hesper, halden, arche, tesh, kael, wanderer, kuehltransport (each with `_fight`) | Act II | calm, tense; fight |
| sterbegaerten, werft, rechenmond, verlies, velm2, brannt, erzstrom, muehle, leitwerk, netz (each with `_fight`) | Acts III and IV | calm, tense; fight |
| klinik, anhoerung, anhoerung_fight | the clinic, the hearing | –; fight |
| boss2, boss3, swarm, final | the machines, the men in frames, the swarms, the last fight | fight, rage |
| hale, ysolde, ilka | while they talk | – |
| act2end, act3end, end_a, end_b, end_c, epilogue | the acts' ends, the endings, the epilogue | – |

**Backgrounds:** a recorded loop per place (`ambience` in the level file: ship, street, coolant, market, depths, suit,
water, vault; Act II's shiphub, ice, clouds, dusk, ark, plasma, snow, transport, alien; Act III's and IV's ring,
clinic, arena, yard, hearing, cryo, heat, siege, cutting, stream, furnace, servers, net) in `assets/sound`; without one the synthesised hum plays as before.

**How it is made** (`tools/make_audio.py`): the pieces are written in [tools/music.json](tools/music.json) and made by
the AudioGenerator's soundtrack mode (ACE-Step 1.5 XL, local; loops cut to the measured beat grid, the tense layer a
cover of the calm one, so both layers of a take share one grid; the fight pieces made on their own: covers barely
follow a changed style, a fight needs a piece of its own). The backgrounds are written in
[tools/ambience.txt](tools/ambience.txt) and made by TextureGen (Stable Audio Open), cut into seamless loops. Both are
levelled against the game's own sounds (`--dumpsound DIR` writes the synthesised ones as WAVs) and stored as OGG
Vorbis, the music at quality 4 (~128 kbit/s), the backgrounds at quality 3: the format raylib plays, small, and
transparent enough for music. The game decodes a piece in the background when it is wanted and mixes the layers
itself (Score.cpp), so layers and takes stay sample-exact. Licences: ACE-Step 1.5 is MIT; Stable Audio Open 1.0 falls
under the Stability AI Community License (to be checked before a sale).

    G:\Tools\PythonSkripts\AudioGenerator\venv\Scripts\python.exe tools\make_audio.py music      # [--encode-only]
    G:\Tools\PythonSkripts\AudioGenerator\venv\Scripts\python.exe tools\make_audio.py ambience   # [--encode-only]

## Textures

    G:\Tools\ImageGeneration\venv\Scripts\python.exe tools\make_textures.py [--force NAME ...] [--only NAME ...]
    G:\Tools\ImageGeneration\venv\Scripts\python.exe tools\make_textures.py --lang en    # only the English signs

Gemini's image model on Vertex AI (the PC's gcloud ADC, project `plenary-office-432719-u6`); Imagen 3 and 4 are gone
(404). The pictures are made seamless, the baked light removed, normal, roughness, metal and occlusion maps derived.
Sign plates are painted once per language from the `sign_...` lines of the text files.

The later places have walls and floors of their own (Game::themeSurfaces): plaster and alley paving in the drums,
hull plates, the Core Bank's ceramic and polished stone, glacial ice and snow, dust and sandstone, the refinery's
rust-red deck, the ark's padded walls, lawn and hedges on the ring, clinic tiles, arena sand and sandstone, yellow
construction plating, veined stone, hoarfrost, scorched plates, regolith, slag, server racks and floors, the net's
grid and its walls of memories. A level may choose others for a theme (`walltex theme texture`, `floortex theme
texture`; the drowned quarter has algae and wet floors). The speakers have portrait paintings
(`tools/make_portraits.py`).
