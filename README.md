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
and the mouse (look). Press **B** to toggle the debug overlay.

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
    UI/       the stats/evolution HUD
Tools/Houdini/  procedural mesh + material generation and headless Unreal import scripts
```

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
| B | Toggle debug overlay (heading arrows + perception circles) |
