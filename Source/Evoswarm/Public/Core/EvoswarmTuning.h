// Copyright Evoswarm.
//
// Central tuning. Stats are abstract "points"; these scales turn a stat value into a
// concrete gameplay quantity (cm/s, hit points, seconds, ...). Authored species ranges
// are expected to sit roughly in the 0..30 band. Tweak freely; nothing else hard-codes
// these numbers.

#pragma once

#include "CoreMinimal.h"
#include "BoidStats.h"

namespace Evo
{
	// --- Arena (UE units are centimetres; 1 km == 100000) ---
	inline constexpr float ArenaHalfExtent = 48000.f;   // half-width of the square arena (~1 km side)
	inline constexpr float FlightZ = 200.f;     // boids are kept on this horizontal plane
	inline constexpr float BoundsMargin = 2500.f;   // start turning back this far from the edge
	inline constexpr float BoundsTurnAccel = 4000.f;  // inward acceleration near the edge

	// --- Stat -> derived quantity scales ---
	inline constexpr float WalkSpeedScale = 60.f;     // cm/s per WalkSpeed point
	inline constexpr float RunSpeedScale = 130.f;    // cm/s per RunSpeed point
	inline constexpr float PerceptionScale = 220.f;    // cm radius per Perception point
	inline constexpr float MaxHPScale = 10.f;     // hit points per HP point
	inline constexpr float MaxStaminaScale = 10.f;     // stamina pool per Stamina point
	inline constexpr float MaxHungerScale = 12.f;     // food capacity per Hunger point
	inline constexpr float BiomassScale = 14.f;     // nutrition delivered to a predator per Biomass point
	inline constexpr float DamageScale = 9.f;      // attack damage per Damage point
	inline constexpr float ArmorScale = 12.f;     // damage soaked per Armor point
	inline constexpr float RegenPerSecScale = 0.5f;     // HP/s per Regeneration point
	inline constexpr float LifespanScale = 14.f;     // seconds of life per Lifespan point

	// --- Metabolism ---
	inline constexpr float HungerDrainPerSec = 1.2f;  // base food burned per second
	inline constexpr float SprintHungerMult = 2.0f;  // extra burn while sprinting
	inline constexpr float CarnivoreHungerMult = 1.7f;  // carnivores burn faster (must keep hunting)
	inline constexpr float StarvationDamagePerSec = 6.f; // HP lost per second at zero hunger
	inline constexpr float StaminaDrainPerSec = 12.f;  // stamina burned per second sprinting
	inline constexpr float StaminaRegenPerSec = 8.f;   // stamina recovered per second cruising

	// --- Rest / sleep ---
	// A boid that is safe and fed but exhausted lies down to recover. Hysteresis on stamina
	// keeps it from flickering in and out of sleep every frame.
	inline constexpr float SleepEnterStaminaFrac = 0.15f; // drops asleep below this fraction of max stamina
	inline constexpr float SleepWakeStaminaFrac = 0.85f;  // wakes once recovered past this fraction
	inline constexpr float SleepMinHungerFrac = 0.25f;    // too hungry to sleep below this fraction of max hunger
	inline constexpr float SleepHungerDrainMult = 0.25f;  // resting burns a quarter of the food
	inline constexpr float SleepRegenMult = 2.0f;         // and heals twice as fast
	inline constexpr float SleepStaminaRegenMult = 2.5f;  // stamina comes back faster lying down

	// --- Reproduction ---
	inline constexpr float MaturityAge = 3.f;   // min seconds before breeding
	inline constexpr float ReproHungerFraction = 0.72f; // fraction of max hunger required to breed
	inline constexpr float ReproHungerCost = 0.45f; // fraction of max hunger spent per birth (each parent)
	inline constexpr float ReproCooldownBase = 12.f; // seconds between births at ReproductionRate 0
	inline constexpr float ReproRateScale = 0.6f; // higher ReproductionRate shortens the cooldown
	inline constexpr float MatingRadius = 700.f;// how close a ready partner must be to breed
	inline constexpr float MateMaxFatigue = 0.8f;// too exhausted to court above this fatigue level

	// --- Genetic algorithm (offspring stat reallocation at birth) ---
	inline constexpr float ReallocStepsScale = 30.f; // mutation steps = 1 + MutationRate * this
	inline constexpr float ReallocStepFraction = 0.3f; // max share of movable budget shifted per step

	// --- Feeding / predation ---
	inline constexpr float EatReach = 200.f; // contact distance to eat food or catch prey
	inline constexpr float FoodEnergy = 35.f;  // hunger restored by one plant (before digestion)
	// Diet is a continuous gradient (0 = herbivore .. 1 = carnivore). It sets how efficiently a
	// boid digests each food type; below this floor it can't use that type at all (true specialists).
	inline constexpr float DietEfficiencyFloor = 0.1f;

	// --- Flocking weights ---
	inline constexpr float SeparationRadius = 350.f;
	inline constexpr float SeparationWeight = 1.6f;
	inline constexpr float AlignmentWeight = 1.0f;
	inline constexpr float CohesionWeightScale = 0.06f; // multiplied by the Integration stat (lower = less clumping)
	inline constexpr float FleeWeight = 4.5f;
	inline constexpr float ChaseWeightScale = 0.5f;  // multiplied by the Aggressiveness stat
	inline constexpr float SeekFoodWeight = 2.5f;  // herbivore pull toward food, scaled by hunger
	inline constexpr float SeekPartnerWeight = 2.5f;
	inline constexpr float MaxSteerAccel = 3000.f;
	inline constexpr float WanderAccel = 600.f;

	// --- Water (land boids avoid it, and wade slowly if they enter) ---
	inline constexpr float WaterLookAhead = 450.f;  // how far ahead a boid checks for water
	inline constexpr float WaterEdgeMargin = 70.f;   // treat ground within this height of the sea as "shore"
	inline constexpr float WaterAvoidAccel = 3200.f; // inland push for POOR swimmers approaching water
	inline constexpr float WaterEscapeAccel = 2400.f; // ALWAYS-on push to shore once in water (no death traps)
	inline constexpr float WaterEscapeProbe = 800.f;  // distance used to find the way to shore
	inline constexpr float WaterWadeSpeedMin = 0.5f;   // swim speed of a non-swimmer (still able to get out)
	inline constexpr float WaterWadeSpeedMax = 0.95f;  // swim speed of a strong swimmer (over water)
	// Aquatic adaptation in [0,1] from the genome (no dedicated gene, so it can evolve):
	// endurance (Stamina) helps, bulk (Biomass) hinders. Strong swimmers ignore water; weak ones avoid it.
	inline constexpr float SwimFromStamina = 0.05f;
	inline constexpr float SwimFromBiomass = 0.018f;
	inline constexpr float SwimBase = 0.10f;

	// --- Movement smoothing & realistic perception ---
	inline constexpr float WanderDriftDegPerSec = 70.f;   // how fast the wander heading meanders (smooth, not jittery)
	inline constexpr float FacingInterpSpeed = 7.f;    // how fast the body turns to face travel (higher = snappier)
	inline constexpr float PerceptionFOVDegrees = 230.f;  // forward field of view; things behind aren't perceived
	inline constexpr float ForceSmoothing = 10.f;   // low-pass on steering force (reduces shake)

	// --- Debug draw (toggled with the B key) ---
	inline constexpr int32 DebugSampleStride = 8;      // draw 1 in N boids, so the view isn't cluttered
	inline constexpr float DebugZLift = 90.f;   // lift overlays above the body so they read clearly
	inline constexpr float DebugArrowThickness = 4.f;
	inline constexpr float DebugCircleThickness = 2.5f;
	inline constexpr float DebugPointSize = 25.f;      // gros points d'état, comme dans la version soutenance
	/** Le texte vitesse/fatigue n'est dessiné qu'à portée de lecture de la caméra : au-delà
	 *  ce n'était de toute façon qu'un pâté illisible, et DrawDebugString coûte cher. */
	inline constexpr float DebugTextMaxDist = 20000.f;
	inline constexpr float DebugTextMaxDistSq = DebugTextMaxDist * DebugTextMaxDist;

	// --- Accélérateur de temps (+ / - / molette, P pour la pause) ---
	// L'évolution se joue sur des centaines de générations : à l'échelle x1 une soutenance
	// n'en montre que quelques-unes. Le paliers sont multiplicatifs pour que chaque cran soit
	// perceptible sans donner l'impression de sauter dans le vide.
	// Le dernier cran s'arrête à x25 : au-delà, le plafond de pas (Evo::MaxSimStep) rendrait
	// la vitesse effective identique à x25 sur toute machine réaliste, et le HUD n'afficherait
	// plus qu'un écart permanent entre demandé et obtenu.
	inline constexpr float SpeedLadder[] = { 0.25f, 0.5f, 1.f, 2.f, 5.f, 10.f, 25.f };
	inline constexpr int32 NumSpeedLevels = UE_ARRAY_COUNT(SpeedLadder);
	inline constexpr int32 DefaultSpeedIndex = 2;   // x1
	/** Au-delà de ce facteur, le rendu s'allège pour rendre des frames plus courtes (voir plus bas). */
	inline constexpr float TurboSpeedThreshold = 2.f;
	/** Sous ce FPS réel soutenu, l'accélérateur redescend d'un cran tout seul. */
	inline constexpr float SpeedFpsFloor = 24.f;
	/** Secondes réelles entre deux auto-régulations (laisse le temps aux réglages de prendre effet). */
	inline constexpr float SpeedThrottleCooldown = 3.f;
	/**
	 * Écrêtage de la frame RÉELLE avant dilatation. UE laisse passer 0,4 s par défaut : à x50
	 * un simple micro-freeze produirait un pas de simulation de 20 secondes, qui téléporterait
	 * toute la population d'un coup. À 0,05 s, un freeze ralentit la simulation au lieu de la
	 * faire diverger.
	 */
	inline constexpr float MaxUndilatedFrame = 0.05f;
	/**
	 * Plafond du pas de simulation, une fois la dilatation appliquée.
	 *
	 * La valeur n'est pas choisie au jugé : elle vient de EatReach. L'alimentation est un test
	 * de proximité (« suis-je à moins de 200 cm de cette plante ? ») évalué une fois par pas.
	 * Si une créature parcourt plus de 200 cm entre deux évaluations, elle peut ENJAMBER sa
	 * nourriture sans jamais entrer dans le test — mesuré : à un pas de 0,35 s la population
	 * s'effondrait de 1092 à 348 individus par famine artificielle.
	 *
	 * On borne donc le déplacement par pas à environ la moitié de EatReach. À la vitesse de
	 * course la plus élevée que produit le génome (~1200 cm/s), 0,08 s valent ~96 cm : la
	 * fenêtre de contact ne peut plus être sautée.
	 *
	 * Conséquence assumée : au-delà d'un certain facteur la vitesse EFFECTIVE plafonne sous la
	 * vitesse demandée, et le HUD affiche les deux. Le mode turbo existe justement pour
	 * relever ce plafond en raccourcissant les frames.
	 *
	 * À noter : ce plafond protège la partie SPATIALE (vol groupé, poursuite, alimentation).
	 * La partie évolutive — faim, vieillissement, délais de reproduction — est linéaire en dt,
	 * donc exacte quel que soit le pas.
	 */
	inline constexpr float MaxSimStep = 0.08f;

	/** Libellé court d'un palier ("x1", "x0.5", "x50"). */
	inline FString SpeedLabel(float Speed)
	{
		return (Speed < 1.f) ? FString::Printf(TEXT("x%.2g"), Speed) : FString::Printf(TEXT("x%g"), Speed);
	}

	// --- Reproduction feedback (born-here flash + soft blip) ---
	inline constexpr float BirthFlashDuration = 0.55f;  // seconds the spawn ring is shown
	inline constexpr float BirthFlashRadius = 170.f; // ring expands to this radius
	inline constexpr float BirthSfxMinInterval = 0.40f; // min seconds between blips (keeps it un-annoying)
	inline constexpr float BirthSfxVolume = 0.13f; // soft

	// --- Terrain follow ---
	inline constexpr float GroundOffset = 60.f;  // how far a boid floats above the surface
	inline constexpr float SlopeSpeedPenalty = 0.6f;  // fraction of speed lost going straight uphill

	// --- Combat (predation) ---
	inline constexpr float IntimidationFleeScale = 0.15f; // prey flee harder from intimidating hunters
	inline constexpr float AttackStaminaCost = 7.f;   // stamina spent per attack
	inline constexpr float AttackHungerCost = 7.f;   // hunger spent per attack (hunting is metabolically costly)
	inline constexpr float AttackCooldownTime = 1.3f;  // seconds between a boid's attacks
	inline constexpr float MinStaminaToAttack = 6.f;   // can't attack below this stamina
	inline constexpr float HuntTierMargin = 0.3f;  // only hunt prey this much lower on the carnivory scale
	inline constexpr float PackRadius = 900.f; // allies within this distance count as a pack
	inline constexpr float PackDamagePerAlly = 0.15f; // +damage per ally, multiplied by Integration
	inline constexpr int32 PackMaxAllies = 6;     // diminishing returns cap
	inline constexpr float CounterDamageScale = 1.1f;  // prey counter-hit = Damage*scale, gated by Aggressiveness
	inline constexpr float AdrenalineDuration = 4.0f;  // seconds of flee burst after being hit
	inline constexpr float AdrenalineSpeedMult = 1.9f;  // speed multiplier while adrenaline lasts

	// --- Carcasses (shared meat from a kill) ---
	inline constexpr float CarcassEnergyScale = 20.f;  // carcass energy = prey Biomass * this (a meal, not a feast)
	inline constexpr float CarcassBite = 55.f;  // meat is worth more than a plant (user-tuned)  // energy a carnivore takes per feeding tick
	inline constexpr float CarcassDecayPerSec = 4.5f;   // carcasses rot away so predators can't hoard food
	inline constexpr float CarcassMeshScale = 0.9f;  // render scale of a carcass

	// --- Food field ---
	inline constexpr int32 FoodTargetCount = 9000;  // arena-wide plant population ceiling (1 km arena)
	inline constexpr int32 FoodSpawnPerTick = 40;    // max regrowths applied per subsystem tick
	// Plants are a finite, slowly renewing resource growing in drifting fertile MEADOW patches
	// (big natural bunches). An eaten plant only regrows after a fallow delay.
	inline constexpr float FoodRegrowDelay = 22.f;   // seconds before an eaten plant regrows...
	inline constexpr float FoodRegrowJitter = 18.f;  // ...plus up to this much random extra
	inline constexpr int32 FoodPatchCount = 26;      // meadows across the arena
	inline constexpr float FoodPatchRadius = 3200.f; // ~64 m bunch radius
	inline constexpr float FoodPatchLifeMin = 120.f; // seconds before a meadow relocates (min...
	inline constexpr float FoodPatchLifeMax = 300.f; // ...and max)
	inline constexpr float FoodMeshScale = 0.8f;  // render scale of a plant instance (grass tuft)

	// --- Live stats & ecosystem events (HUD data layer) ---
	inline constexpr float StatsSampleInterval = 0.5f;   // seconds between history samples (pop / births / deaths)
	inline constexpr int32 StatsHistorySamples = 160;    // ring-buffer length (~80 s of history at 0.5 s)
	inline constexpr float RateWindowSec = 30.f;   // window used to compute births/deaths per minute
	inline constexpr int32 EventLogMaxEntries = 64;     // oldest events dropped past this
	inline constexpr int32 GenMilestoneStep = 10;     // log an event every N generations reached
	inline constexpr float CollapseWindowSec = 30.f;   // population-collapse detection window
	inline constexpr float CollapseFraction = 0.4f;   // collapse = pop falls below this fraction of the window start
	inline constexpr int32 CollapseMinPop = 40;     // collapses only meaningful from this population
	inline constexpr float CollapseCooldownSec = 60.f;   // min seconds between collapse events per species
	// Diet classification thresholds (+ hysteresis so a species doesn't flip-flop at the border).
	inline constexpr float DietCarnThreshold = 0.65f;  // avg diet above this = carnivore
	inline constexpr float DietHerbThreshold = 0.35f;  // avg diet below this = herbivore
	inline constexpr float DietClassHysteresis = 0.05f;  // must cross the border by this much to switch class

	// --- Analytics dashboard (G overlay) ---
	// The timeline covers the WHOLE run in fixed memory: once it holds this many samples it
	// merges adjacent pairs and halves its time resolution, forever. 900 samples x ~310 bytes
	// is ~280 KB per species, so raise it freely if you want finer old history.
	inline constexpr int32 TimelineMaxSamples = 900;
	inline constexpr int32 ScatterMaxPoints = 260;   // individuals sampled per species for the scatter page
	inline constexpr float ChartFillStridePx = 3.f;   // px step used to fill spread / stacked-area bands
	inline constexpr float FlowRateWindowSec = 20.f;  // window for food-web energy-throughput rates

	// --- Creature appearance (every individual's genome shapes how it looks) ---
	// Colour = diet hue x vigour brightness; one instanced mesh per (hue, shade) bucket.
	inline constexpr int32 NumDietHues = 10;
	inline constexpr int32 NumShades = 3;
	inline constexpr int32 NumAppearanceBuckets = NumDietHues * NumShades;
	inline constexpr float VigorNorm = 45.f;  // normalises HP+Armor+Damage into [0,1]
	// Body shape: each axis reflects a different stat, so silhouettes vary per individual.
	inline constexpr float BodySizeBase = 0.5f;  // baseline bulk
	inline constexpr float BodySizeFromHP = 0.03f; // bulkier with HP
	inline constexpr float BodyStreamlineFromSpeed = 0.022f; // longer/leaner with run speed
	inline constexpr float BodyWidthFromArmor = 0.03f; // broader with armour

	// --- Procedural gait (render-only; never written back into the simulation) ---
	// The phase is integrated per-entity in the movement processor and consumed by the render
	// processor, which offsets the LOCAL render transform only. Every amplitude is gated by
	// normalised speed, so a walk is subtle and a sprint emphatic from a single formula.
	inline constexpr float GaitStrideBase = 80.f;   // cm travelled per footfall at body length 1
	inline constexpr float GaitStrideFromLength = 70.f;   // longer bodies take longer, slower strides
	inline constexpr float GaitBobAmp = 16.f;   // cm of vertical bounce at full sprint
	inline constexpr float GaitRollAmpDeg = 7.f;    // side-to-side sway (degrees) at full sprint
	inline constexpr float GaitPitchAmpDeg = 4.5f;  // nose dip per footfall (degrees)
	inline constexpr float GaitSquash = 0.07f; // squash/stretch fraction per step
	inline constexpr float GaitBobFromBiomass = 0.020f; // heavy creatures bounce less, per Biomass point
	inline constexpr float GaitSpeciesSpread = 0.15f; // +/- stride variation between species
	inline constexpr float GaitLimpHPFraction = 0.35f; // below this HP fraction the gait goes uneven
	inline constexpr float GaitLimpAmount = 0.55f; // how far the weak leg collapses
	inline constexpr float GaitStaggerAmpDeg = 7.f;    // extra roll wobble while starving
	inline constexpr float GaitIdleSpeedFrac = 0.06f; // below this fraction of run speed = idle
	inline constexpr float GaitBreatheAmp = 0.018f;// idle (awake) breathing scale amplitude
	inline constexpr float GaitBreatheFreq = 1.2f;  // idle (awake) breathing rad/s
	inline constexpr float GaitSleepBreatheAmp = 0.045f;// asleep: deeper breaths, clearly readable
	inline constexpr float GaitSleepBreatheFreq = 0.45f; // asleep: and much slower
	inline constexpr float GaitStaggerFreq = 1.7f;  // starvation wobble rad/s (deliberately off-beat)
	inline constexpr float GaitMaxPhaseStep = 1.2f;  // rad/frame above which the gait fades (aliasing guard)

	// Derived helpers ---------------------------------------------------------
	inline float MaxHP(const FBoidGenome& G) { return FMath::Max(1.f, G.Get(EBoidStat::HP) * MaxHPScale); }
	inline float MaxStamina(const FBoidGenome& G) { return FMath::Max(1.f, G.Get(EBoidStat::Stamina) * MaxStaminaScale); }
	inline float MaxHunger(const FBoidGenome& G) { return FMath::Max(1.f, G.Get(EBoidStat::Hunger) * MaxHungerScale); }
	inline float WalkSpeed(const FBoidGenome& G) { return FMath::Max(1.f, G.Get(EBoidStat::WalkSpeed) * WalkSpeedScale); }
	inline float RunSpeed(const FBoidGenome& G) { return FMath::Max(WalkSpeed(G), G.Get(EBoidStat::RunSpeed) * RunSpeedScale); }
	inline float PerceptionRadius(const FBoidGenome& G) { return FMath::Max(100.f, G.Get(EBoidStat::Perception) * PerceptionScale); }
	inline float Lifespan(const FBoidGenome& G) { return FMath::Max(5.f, G.Get(EBoidStat::Lifespan) * LifespanScale); }

	/**
	 * Exhaustion in [0,1]: 0 = fully rested, 1 = spent. Derived from stamina rather than
	 * stored, so there is only ever one thing to keep up to date (the movement processor
	 * already drains and regenerates stamina, and sleep restores it faster).
	 */
	inline float Fatigue(const FBoidGenome& G, float CurrentStamina)
	{
		return FMath::Clamp(1.f - CurrentStamina / MaxStamina(G), 0.f, 1.f);
	}
	inline float ReproCooldown(const FBoidGenome& G)
	{
		return ReproCooldownBase / (1.f + FMath::Max(0.f, G.Get(EBoidStat::ReproductionRate)) * ReproRateScale);
	}
	// --- Diet as a continuous gradient ---------------------------------------
	// Efficiency in [0,1] of extracting energy from each food type, crossfading with Diet.
	inline float PlantDigestionFromDiet(float Diet) { return FMath::Clamp(1.f - Diet, 0.f, 1.f); }
	inline float MeatDigestionFromDiet(float Diet) { return FMath::Clamp(Diet, 0.f, 1.f); }
	inline float PlantDigestion(const FBoidGenome& G) { return PlantDigestionFromDiet(G.Get(EBoidStat::Diet)); }
	inline float MeatDigestion(const FBoidGenome& G) { return MeatDigestionFromDiet(G.Get(EBoidStat::Diet)); }

	// Whether it's worth bothering with a food type at all (specialists ignore the other one).
	inline bool CanEatPlants(const FBoidGenome& G) { return PlantDigestion(G) > DietEfficiencyFloor; }
	inline bool CanHunt(const FBoidGenome& G) { return MeatDigestion(G) > DietEfficiencyFloor; }

	/** How well an individual handles water in [0,1]: endurance helps, bulk hinders. Evolvable. */
	inline float AquaticAdaptation(const FBoidGenome& G)
	{
		return FMath::Clamp(SwimBase + SwimFromStamina * G.Get(EBoidStat::Stamina) - SwimFromBiomass * G.Get(EBoidStat::Biomass), 0.f, 1.f);
	}

	// --- Appearance from genome ---------------------------------------------
	/** Colour along the diet gradient: green (herbivore) -> yellow (omnivore) -> red (carnivore). */
	inline FLinearColor DietColor(float Diet)
	{
		const float D = FMath::Clamp(Diet, 0.f, 1.f);
		const FLinearColor Herb(0.20f, 0.72f, 0.22f);
		const FLinearColor Omni(0.90f, 0.80f, 0.18f);
		const FLinearColor Carn(0.86f, 0.14f, 0.12f);
		return (D < 0.5f) ? FMath::Lerp(Herb, Omni, D * 2.f) : FMath::Lerp(Omni, Carn, (D - 0.5f) * 2.f);
	}

	/** Overall robustness in [0,1] from offensive/defensive stats — drives colour brightness. */
	inline float Vigor(const FBoidGenome& G)
	{
		return FMath::Clamp((G.Get(EBoidStat::HP) + G.Get(EBoidStat::Armor) + G.Get(EBoidStat::Damage)) / VigorNorm, 0.f, 1.f);
	}

	inline int32 HueBucket(float Diet) { return FMath::Clamp(static_cast<int32>(FMath::Clamp(Diet, 0.f, 1.f) * NumDietHues), 0, NumDietHues - 1); }
	inline int32 ShadeBucket(float V) { return FMath::Clamp(static_cast<int32>(V * NumShades), 0, NumShades - 1); }

	/** Which appearance bucket (hue x shade) a creature is drawn in. */
	inline int32 AppearanceBucket(const FBoidGenome& G)
	{
		return ShadeBucket(Vigor(G)) * NumDietHues + HueBucket(G.Get(EBoidStat::Diet));
	}

	/** Colour for a given (hue, shade) bucket: diet hue, darkened/brightened by vigour. */
	inline FLinearColor BucketColor(int32 Hue, int32 Shade)
	{
		const float Diet = (Hue + 0.5f) / NumDietHues;
		const float Bright = (NumShades > 1) ? FMath::Lerp(0.55f, 1.45f, static_cast<float>(Shade) / (NumShades - 1)) : 1.f;
		return DietColor(Diet) * Bright;
	}

	/**
	 * The Houdini meshes import Y-up (their tall axis is local +Y), but Unreal is Z-up. This is the
	 * rotation that stands them upright (+Y -> +Z). Deterministic, so no guessing the sign.
	 */
	inline FQuat MeshStandUp()
	{
		return FQuat::FindBetweenNormals(FVector(0.f, 1.f, 0.f), FVector(0.f, 0.f, 1.f));
	}

	/** Per-individual body shape: bulk from HP, length from run speed, width from armour. */
	inline FVector BodyScale(const FBoidGenome& G)
	{
		const float Size = BodySizeBase + BodySizeFromHP * G.Get(EBoidStat::HP);
		const float Length = 1.2f + BodyStreamlineFromSpeed * G.Get(EBoidStat::RunSpeed);
		const float Width = 0.7f + BodyWidthFromArmor * G.Get(EBoidStat::Armor);
		return FVector(Size * Length, Size * Width, Size * 0.7f);
	}

	// --- Gait helpers (must follow BodyScale, which StrideLength depends on) --------------
	/**
	 * Distance in cm travelled per footfall. Scales with the individual's fore-aft body extent,
	 * so large creatures take long slow strides and small ones scurry. Silhouette and locomotion
	 * stay consistent because both derive from BodyScale.
	 */
	inline float StrideLength(const FBoidGenome& G)
	{
		return FMath::Max(25.f, GaitStrideBase + GaitStrideFromLength * BodyScale(G).X);
	}

	/**
	 * Deterministic per-species stride multiplier in [1-Spread, 1+Spread], so a herbivore herd
	 * reads visibly differently from a predator pack even at the same speed. Hash-based: stable
	 * across runs, no state, and no table to maintain when species are added.
	 */
	// --- Day / night sky clock (published by the game mode's GoodSky driver; VISUAL only) ---
	EVOSWARM_API extern float GDaylight; // 1 = day, 0 = deep night
	EVOSWARM_API extern float GSkyHour;  // hour of day in [0,24)
	/** Daylight [0,1] from an hour of day: dark before 5h / after 21h, full 8h..18h. */
	inline float DaylightFromHour(float Hour)
	{
		const float Dawn = FMath::SmoothStep(5.f, 8.f, Hour);
		const float Dusk = 1.f - FMath::SmoothStep(18.f, 21.f, Hour);
		return FMath::Clamp(FMath::Min(Dawn, Dusk), 0.f, 1.f);
	}

	/**
	 * How awake a creature is right now, in [0,1], from its species' Nocturnality
	 * (0 = diurnal, 1 = nocturnal) and the current daylight. Diurnal peaks by day, nocturnal
	 * by night; both keep a floor so an off-clock animal can still react to danger.
	 */
	inline float Activity(float Nocturnality, float Daylight)
	{
		const float Preferred = FMath::Lerp(Daylight, 1.f - Daylight, FMath::Clamp(Nocturnality, 0.f, 1.f));
		return FMath::Lerp(0.25f, 1.f, Preferred);
	}

	// --- Steering level-of-detail (by distance to the camera) -----------------
	// The neighbourhood query dominates the sim's cost. Boids near the camera re-decide often
	// (smooth on-screen motion); distant, mostly off-screen boids re-decide far less and coast
	// on their last force. MOVEMENT still integrates every frame, so nothing teleports --
	// only the decision RATE drops with range.
	inline constexpr float LodNearDist = 18000.f;  // < ~180 m: recompute every other frame
	inline constexpr float LodMidDist  = 40000.f;  // < ~400 m: every 4th frame
	// beyond LodMidDist: every 8th frame

	/** Frames between steering recomputes for a boid this far (squared dist) from the camera. */
	inline int32 SteerStride(float DistSq)
	{
		if (DistSq < LodNearDist * LodNearDist) return 2;
		if (DistSq < LodMidDist * LodMidDist)   return 4;
		return 8;
	}

	inline float SpeciesGaitScale(int32 SpeciesIndex)
	{
		const float H = FMath::Frac(FMath::Sin((SpeciesIndex + 1) * 12.9898f) * 43758.5453f);
		return 1.f + (H * 2.f - 1.f) * GaitSpeciesSpread;
	}
}