# Evoswarm

A data-oriented simulation of **evolving creatures** built on Unreal Engine 5.7's
[Mass Entity](https://docs.unrealengine.com/5.3/en-US/overview-of-mass-entity-in-unreal-engine/)
framework. Four species are seeded across a procedurally generated, biome-varied world where
they flock, forage, hunt, flee, **choose mates**, and **evolve** — every birth recombining and
mutating its two parents' stat allocation under natural (and sexual) selection.

The world — terrain, sky, lighting, water, and creatures — assembles itself in C++ on `Play`.
The stylized low-poly props (trees, rocks, plants, water) are generated procedurally in **Houdini**
and imported as meshes; everything else is built at runtime.

## Quick start

Requirements: **Unreal Engine 5.7** and Visual Studio 2022 with the C++ game-development toolchain.

1. Build the **Development Editor | Win64** target (or open the `.uproject` and accept the prompt
   to compile the `Evoswarm` module).
2. Open the project, open any level, and press **Play**.

The game mode clears the level and builds the whole world procedurally, so any level works (an
empty one is cleanest). Fly the spectator camera with **ZQSD** (move), **Space / Ctrl** (up/down),
and the mouse (look). Press **B** to toggle the debug overlay, **+ / -** to accelerate time.

### Optional: the GoodSky pack

The day/night cycle is driven by [GoodSky](https://www.fab.com/), a paid Fab pack that is **not
committed** to this repository. Install it into `Content/GoodSky/` and it is picked up
automatically — C++ owns the clock and pushes the hour into the dome each tick, so the sky, the
sun's rotation, and the fog colour stay in step.

Without it the project still builds and runs: `SpawnGoodSky` logs a warning, and the engine sky,
the directional sun, and the day/night *gameplay* cycle (perception and stamina keyed to
`Evo::Activity`) all keep working. Only the horizon looks plainer.

### Packaging

```
RunUAT.bat BuildCookRun -project=<path>\EvoSwarm.uproject -noP4 -platform=Win64 ^
  -clientconfig=Development -build -cook -map=/Game/evoswarm -stage -pak -archive ^
  -archivedirectory=<path>\Packaged
```

Package **Development**, not Shipping: `ENABLE_DRAW_DEBUG` is compiled out in Shipping, which
silently strips every `DrawDebug*` call and makes the whole debug overlay (keys 0–4 and B) look
broken. Assets loaded by runtime path also need `+DirectoriesToAlwaysCook` entries in
`Config/DefaultGame.ini`, or the packaged world comes up flat and untextured.

## How it works

### The genome

Each creature carries an `FBoidGenome`: a flat array of 18 evolvable stats (HP, armour, walk/run
speed, stamina, regeneration, hunger capacity, biomass, stealth, damage, intimidation,
aggressiveness, perception, diet, lifespan, reproduction rate, integration, mutation rate). Each
species defines a per-stat range and **cost weight**, plus a total point **budget** — spending on
an expensive stat consumes more of the budget, so a genome is always a trade-off. Diet is a
continuous gradient (0 = herbivore … 1 = carnivore) that smoothly sets how efficiently a creature
digests plants vs. meat. Some traits are *derived* rather than stored — e.g. **aquatic adaptation**
comes from stamina (endurance) minus biomass (heft), so swimming ability can evolve with no
dedicated gene.

### Evolution (the genetic algorithm)

Reproduction is **sexual**. A mature, well-fed, rested individual searches the spatial grid for the
**fittest ready partner of its own species nearby** (sexual selection by body condition), and the
two breed together:

- **Crossover** — each stat is inherited from one parent or the other (uniform crossover).
- **Budget-conserving mutation** — instead of nudging every stat, mutation *moves points between
  stats at their real costs*, keeping the total budget exact; drop a point of a cheap stat and you
  can only buy a sliver of an expensive one. Cost-free traits (diet, mutation rate) drift freely.
- Both parents pay a hunger cost and go on cooldown; the child is born between them, one generation
  up.

Selection is implicit — whoever survives, stays healthy, and finds a mate passes on their traits.
The HUD plots population over time and per-species average stats (with a diet gradient bar) so the
drift is observable generation to generation.

### Appearance from the genome

Every individual is drawn from its own stats, so you can read the population at a glance and watch
it shift as it evolves:

- **Colour** — hue from **diet** (green herbivore → red carnivore), brightness from **vigour**
  (HP + armour + damage), binned into instanced-mesh buckets.
- **Body shape** — bulk from **HP**, length from **run speed**, width from **armour** (continuous,
  per-instance).

### Behaviour

Per-frame logic runs as a chain of Mass processors:

```
GridUpdate → Steering → Movement → Feeding → FoodDecay → Metabolism → Reproduction → Stats → Render
```

- **Steering** — separation/alignment/cohesion flocking with a forward field-of-view and distance
  weighting, plus predator/prey forces, food-seeking, and adaptation-scaled water avoidance.
- **Movement** — integrates steering into a smoothed velocity and walks the creature along the
  terrain surface, leaning into slopes, slowing uphill, and slowing in water (by how poorly it
  swims).
- **Feeding** — herbivores graze plants; carnivores hunt live prey (stamina-gated, pack-boosted,
  with prey counter-attacks and an adrenaline flee burst); a kill drops a carcass the pack and
  scavengers share before it rots.
- **Metabolism / Reproduction** — hunger, regeneration, ageing, death, and mate-based breeding
  under a per-species carrying-capacity cap. Each birth spawns a small ring + soft chime.

The cross-entity interaction pattern is consistent: each frame the grid publishes a cheap snapshot
(position, species, "ready to mate", condition…); a processor queries it for a match, reaches into
the partner's fragments via `GetFragmentDataPtr`, and defers any birth/death to the sim subsystem.

### World & water

Terrain is a procedural Perlin heightfield split into four organic biomes — **grassland**,
**forest**, **desert**, **highlands** (snow-capped peaks) — each affecting food density, movement,
metabolism, perception, and concealment, with shoreline **sand** and steep-slope **rock** shading.
Basins below sea level fill with **proper water**: a translucent, glossy surface that creatures can
*swim across* — freely if well-adapted (high stamina, low bulk), reluctantly and slowly if not — so
lakes shape where populations can spread without walling anyone off. Sky (atmosphere, volumetric
clouds, fog), lighting, and a post-process grade are all set up in code.

### Day and night

C++ owns the clock: it advances the hour, rotates the sun, and pushes both into the GoodSky dome,
then re-reads the dome's own horizon colour to tint the exponential height fog at 4 Hz — which is
what keeps the pale band at the horizon from separating from the sky. Daylight is published as
`Evo::GDaylight` / `Evo::GSkyHour` and feeds gameplay through `Evo::Activity()`: each species has a
`Nocturnality` weight, so diurnal creatures lose perception and recover stamina poorly at night
while nocturnal ones do the opposite.

### Low-poly art (Houdini)

The trees, rocks, bushes, cactus, flowers, grass, carcass, and water mesh are generated
procedurally with Houdini and imported as static meshes under `Content/EvoGen/`. The generator and
import scripts live in `Tools/Houdini/` (`gen_assets.py`, `import_fbx.py`, `make_water_mat.py`), so
the assets are fully reproducible — tweak the script and re-run.

## Technical notes

### Why Mass / data-oriented

A classic `AActor`-per-creature design pairs each agent's data and logic in one heap object;
with thousands of agents that fragments memory and thrashes the cache. Mass is an **ECS**
(Entity-Component-System) that separates the three concerns:

- **Entity** — just an id (`FMassEntityHandle`); it owns no logic.
- **Fragment** — a small POD struct of data attached to an entity (`FBoidGenomeFragment`,
  `FBoidStateFragment`, …). Entities with the same set of fragments share an **archetype** and
  are stored together in contiguous **chunks**, one tight array per fragment — so a processor
  streaming "all velocities" walks linear memory.
- **Processor** (`UMassProcessor`) — the per-frame logic. Each declares an `FMassEntityQuery`
  of the fragments/tags it needs and at which access level (read-only vs read-write); Mass hands
  it only the matching chunks. Processors are ordered with `ExecuteAfter` to form the chain above.

Data that is genuinely shared per species (config pointer, species index, base colour) lives in a
**shared fragment**, deduplicated to one copy per species; everything that varies per individual
(genome, HP, hunger, age) is a normal per-entity fragment. That split is what lets one body of
code drive thousands of agents while each evolves, starves, and dies independently.

### Talking between entities

A processor only sees the entity it's iterating, so cross-agent behaviour (mating, predation)
uses a three-step pattern through `UBoidGridSubsystem`, a uniform spatial hash rebuilt each frame:

1. **Publish** — the grid-update processor writes a cheap per-agent snapshot (position, species,
   biomass, stealth, `bCanMate`, body `Condition`, …) into hash cells.
2. **Query** — another processor asks the grid for neighbours in a radius and picks a target
   (nearest prey, fittest ready mate) and **claims** it (`TryClaim`) so two agents can't both act
   on the same partner/prey in one frame.
3. **Act** — it reaches into the target's fragments via `EntityManager.GetFragmentDataPtr<T>()`
   to read/modify it, and **defers** any structural change (births, carcasses, deaths) to
   `UEvoswarmSimSubsystem`, which flushes them on its own tick — never mutating entity composition
   mid-iteration.

### The budget-conserving genetic operator

Each species has a point **budget**; a stat's cost is `(value − min) × costWeight`, summed across
the genome. Crossover can land a child above that frontier, so it's clamped back, then mutation
**reallocates** rather than perturbs: it repeatedly picks a donor and recipient stat and moves a
budget amount `b` between them — the donor loses `b / costWeight_donor` of value, the recipient
gains `b / costWeight_recipient` — so the total spend is invariant and expensive stats are
genuinely expensive to raise. Cost-free traits (diet, mutation rate) jitter within their range.

### Rendering

Boids and props are drawn as **instanced static meshes** (`UInstancedStaticMeshComponent`): the
render processor rebuilds each frame's transforms and pushes them in one batch
(`BatchUpdateInstancesTransforms`), falling back to a full rebuild only when the instance count
changes. Per-individual colour can't come from a single material parameter, so boids are binned
into a small grid of diet-hue × vigour-shade ISMs; per-individual **size/shape** is free (it's
just the instance transform's scale). Imported meshes are Y-up, so a deterministic
`FQuat::FindBetweenNormals(+Y, +Z)` stands them upright at placement.

### Houdini asset pipeline

`Tools/Houdini/gen_assets.py` runs under `hython` (Houdini's headless Python) to build faceted
low-poly meshes from SOP networks and export FBX; `import_fbx.py` is executed by a headless
`UnrealEditor-Cmd` instance to import them into `Content/EvoGen/` at the right scale; and
`make_water_mat.py` builds the translucent water material via `MaterialEditingLibrary`. No GUI
step is involved, so regenerating the art is one command.

## Source layout

```
Source/Evoswarm/
  Public|Private/
    Core/     module + central tuning constants (EvoswarmTuning.h)
    Genome/   the genome, stat definitions, cost/budget/crossover/mutation maths, species config
    Mass/     fragments, the spatial grid, and all per-frame processors
    World/    terrain, water, simulation subsystem, spawning, game mode, renderer, pawn
    UI/       the stats/evolution HUD, the analytics dashboard, and CSV export
Source/EvoSwarmFlock/   a second module kept from the merge of the two prototypes
Tools/Houdini/  procedural mesh + material generation and headless Unreal import scripts
Tools/compare_branches.sh   whitespace-insensitive diff between two versions of the project
```

`Core/` and `Unity/` at the repository root are the earlier **FlockForge** C#/Unity prototype,
kept for history; they are not part of the Unreal build.

Almost all tuning lives in `Core/EvoswarmTuning.h` (gameplay scales, appearance, water) and
`World/EvoswarmTerrain.h` (terrain and biome parameters). Species ranges, costs, budgets, colours,
and population caps are defined per species; the four defaults are built in
`EvoswarmGameMode::MakeDefaultSpecies`, or you can author `USpeciesConfig` data assets instead.

## Controls

| Input | Action |
|---|---|
| Z / Q / S / D | Move (forward / left / back / right) |
| Space / Left Ctrl | Up / down |
| Mouse | Look |
| B | Toggle the debug overlay (lands on mode 2 if none is selected) |
| Numpad 0–4 | Pick the overlay mode — see below. A mode key also switches the overlay on |
| **+ / −** (numpad, top row, or mouse wheel) | Time accelerator: one step up / down |
| **P** | Pause / resume the simulation (the camera stays free) |
| F | Lock / unlock the crosshair inspector on a creature |
| H | Show / hide the stats panel |
| G | Open / close the analytics dashboard |
| Tab | Next dashboard page |
| [ / ] | Cycle the current page's selection (charted trait, or scatter axis pair) |
| K | Export the recorded run to CSV under `Saved/Evoswarm/` |

### Debug overlay modes

| Mode | Shows |
|---|---|
| 0 | Nothing (overlay off) |
| 1 | A large dot per creature, coloured by behaviour state |
| 2 | Mode 1 + speed/fatigue text + arrows to what each creature is actually chasing |
| 3 | Species-coloured dots + target arrows + HP / hunger bars |
| 4 | Perception circles and heading arrows (one creature in eight) |

State colours: green wandering, yellow foraging, orange hunting, red fleeing, magenta courting,
black asleep. Target arrows are yellow for a plant, orange for a carcass, thick red for live prey,
magenta for a mate.

The arrows read a target the steering processor already computed and cached on the state fragment.
Re-querying the spatial grid per creature per frame just to draw them — the obvious implementation —
froze the editor outright.

## Time accelerator

Evolution is the point of the simulation, and at ×1 a demo shows only a handful of generations.
`+` / `−` step through ×0.25, ×0.5, **×1**, ×2, ×5, ×10, ×25; `P` pauses.

Pause uses a near-zero time dilation rather than `SetGamePaused`, so the world freezes while the
camera, HUD, and dashboard stay live — you can fly around and comment on a still frame. The camera
is compensated (`CustomTimeDilation = 1 / factor`) so it moves at real-world speed at every setting.

Above ×2 a **turbo** mode trims the renderer (volumetric fog, Lumen, shadow distance, resolution
scale). That is not a "prettiness for speed" trade: the simulation advances by *frame duration ×
factor*, and that step is capped, so **shorter frames are the only way to actually reach the
requested factor**. Turbo never engages at ×1, so the reference image is never degraded.

The step cap is derived from `Evo::EatReach`, not guessed. Feeding is a proximity test evaluated
once per step ("am I within 200 cm of this plant?"); a creature that travels further than that
between two evaluations can **step straight over its food** without ever entering the test. Measured
with a 0.35 s cap, the population starved from 1092 down to 348 in open pasture. Capping the step at
0.08 s — about 96 cm of travel — turned the same run into growth from 886 to 1329.

A consequence worth knowing: past a certain factor the *effective* speed plateaus below the
requested one, and the HUD shows both (`x25 -> x8`) rather than claiming a factor it isn't
delivering. The cap protects **spatial** behaviour (flocking, pursuit, feeding); hunger, ageing, and
reproduction cooldowns are linear in `dt` and therefore exact at any step, so evolutionary results
stay valid at speed.

## Instrumentation

`Saved/Logs/EvoPerf.csv` is written every two real seconds: frame time, FPS, per-species
populations, food count, pending queues, and resident memory. Disable it with `-EvoNoPerfLog`.

`Tools/compare_branches.sh` diffs two versions of the project (zip or folder) while ignoring the
reformatting noise that otherwise buries real changes:

```bash
./Tools/compare_branches.sh -o report.txt path/to/A.zip path/to/B.zip
```

### Command-line flags

| Flag | Effect |
|---|---|
| `-EvoSpeed=N` | Start at the ladder step nearest `N` — handy for scripting a demo |
| `-EvoFloraMode=0\|1\|2` | Decor strategy: 0 plain ISM (default), 1 adds cull distances and a shadow policy, 2 adds HISM |
| `-EvoNoPerfLog` | Skip the performance CSV |
| `-EvoScreenshot` | Take one framed still and exit |

`-EvoFloraMode` exists because the obvious optimisation did not survive measurement. Same binary,
matched populations, averaged over ~35 s of simulation: mode 0 ran at **15.25 ms**, mode 2 at
**16.39 ms**, mode 1 at **16.85 ms**. The decor uses a trivial material and was never the
bottleneck, so evaluating a cull distance per instance — let alone maintaining a hierarchical tree —
costs more CPU each frame than it saves on the GPU. The default is therefore the original setup, and
the other two are kept so the measurement can be repeated on weaker hardware, where the verdict may
flip.
