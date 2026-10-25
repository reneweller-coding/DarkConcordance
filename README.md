# SpaceOpera (working title)

A dark space opera action RPG in the manner of *Baldur's Gate: Dark Alliance*: fixed three-quarter camera, gamepad,
ranged combat aimed as in *Fallout: Brotherhood of Steel*, pools of light in the dark. The story is the user's outline
(*DARK CONCORDANCE* / *KATABASIS*); the production plan is in [docs/PLAN.md](docs/PLAN.md).

## Level 1 „Die Kühlgedärme“ (playable prototype)

Heat-exchanger tunnels near Velm's axis: little gravity, fog, rust-red emergency light. Corvin Aske with his Gauss rifle
against rust lice and welder lice, across a cooling shaft, past the cold store with the empty bodies, to the drone
press that builds new lice from scrap.

- **Low gravity:** long jumps; every shot pushes Corvin back, on the ground a little, in the air a lot.
- **Aiming:** without aiming the rifle takes the nearest target ahead (as in Brotherhood of Steel); the mouse or the
  right stick aim by hand, with a little assist. The rifle heats up.
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

Move with WASD, the arrows or the left stick; jump with Space or A; shoot with the left mouse button, the right trigger
or X; Q or Y injects a stim; the mouse wheel or the shoulder buttons zoom; Esc or Start pauses (difficulty,
brightness, restart, quit); F1 frame rate, F12 picture.

The level is a text file, [assets/levels/kuehlgedaerme.txt](assets/levels/kuehlgedaerme.txt): an ASCII map of 2 m
cells (legend at its top), zones with their themes, signs, the spoken lines and the hints with their triggers.

![The shaft](docs/screenshots/l1_jump.jpg)
![The condensate swarm](docs/screenshots/l1_drops.jpg)
![The drone press](docs/screenshots/l1_press.jpg)

## Look test (`--lookdev`)

The first room: an observation deck with a glass floor over a gas giant and a nebula, sun shafts, a monolith.

![Monolith and the glass floor](docs/screenshots/final_a.jpg)

## Build and run

    .\build.ps1          # msvc Release; the game lands in bin\msvc\SpaceOpera.exe

Tests and pictures without a window (always silent):

    bin\msvc\SpaceOpera.exe --shot work\a.png --autoplay --report --warm 900   # the autopilot plays the level
    bin\msvc\SpaceOpera.exe --shot work\b.png --at 25,25 --face 180 --awake --fire --warm 4
    bin\msvc\SpaceOpera.exe --lookdev --shot work\c.png --at 0.6,-4.3 --face 145 --walk

`--report` writes the run (finished, time, deaths, kills, shots) into `spaceopera.log` beside the program; `--bench n`
renders n frames first and logs the time per frame. `--autoplay` without `--shot` lets the autopilot play in the window. `--difficulty leicht|normal|schwer` overrides the saved setting; `--menu` puts the title menu over a `--shot` picture.

## Textures

    G:\Tools\ImageGeneration\venv\Scripts\python.exe tools\make_textures.py [--force NAME ...] [--only NAME ...]

Gemini's image model on Vertex AI (the PC's gcloud ADC, project `plenary-office-432719-u6`); Imagen 3 and 4 are gone
(404). The pictures are made seamless, the baked light removed, normal, roughness, metal and occlusion maps derived.
