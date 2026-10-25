# Plan

## Look test (25.10.2026) — done

One room, see the README. Findings:

- **Environment:** generated textures plus shapes built in code carry the look: detailed surfaces, light, fog and the
  view through the glass floor. Hard-surface sci-fi suits this well.
- **Hero:** the weakest part. Rounded boxes and procedural animation read at this distance, but look like a toy up
  close; the game needs real character models (CC0 packs with skeletons, or modelled) for humans. Robots, drones and
  creatures can stay procedural.
- **Speed:** 5.7 ms a frame at 1920x1080 with 1.5x supersampling on the RTX 5090. The Steam Deck will need the fog at
  half resolution, fewer fog steps and no supersampling.
- **3D generator:** not used (the user: it never generated anything usable).

## Next

1. The user's background story: worlds per act, factions (enemy types), heroes (classes), the mystery, ships.
2. Vertical slice: one level of the abandoned station, one class, three enemy types, combat feel (auto-aim as in
   Brotherhood of Steel, optional fine aim with the right stick), loot, a door, a boss room.
3. Characters: decide where human models come from.
4. Then classes, levelling, a hub with a trader, co-op for two, further acts, music from the VRAudio generators.
