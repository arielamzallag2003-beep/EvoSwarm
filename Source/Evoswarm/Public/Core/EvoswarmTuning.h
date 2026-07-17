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
	inline constexpr float ArenaHalfExtent  = 12000.f;   // half-width of the square arena (~240 m)
	inline constexpr float FlightZ          = 200.f;     // boids are kept on this horizontal plane
	inline constexpr float BoundsMargin      = 2500.f;   // start turning back this far from the edge
	inline constexpr float BoundsTurnAccel    = 4000.f;  // inward acceleration near the edge

	// --- Stat -> derived quantity scales ---
	inline constexpr float WalkSpeedScale    = 60.f;     // cm/s per WalkSpeed point
	inline constexpr float RunSpeedScale     = 130.f;    // cm/s per RunSpeed point
	inline constexpr float PerceptionScale   = 220.f;    // cm radius per Perception point
	inline constexpr float MaxHPScale        = 10.f;     // hit points per HP point
	inline constexpr float MaxStaminaScale   = 10.f;     // stamina pool per Stamina point
	inline constexpr float MaxHungerScale    = 12.f;     // food capacity per Hunger point
	inline constexpr float BiomassScale      = 14.f;     // nutrition delivered to a predator per Biomass point
	inline constexpr float DamageScale       = 9.f;      // attack damage per Damage point
	inline constexpr float ArmorScale        = 12.f;     // damage soaked per Armor point
	inline constexpr float RegenPerSecScale  = 0.5f;     // HP/s per Regeneration point
	inline constexpr float LifespanScale     = 14.f;     // seconds of life per Lifespan point

	// --- Metabolism ---
	inline constexpr float HungerDrainPerSec    = 1.2f;  // base food burned per second
	inline constexpr float SprintHungerMult     = 2.0f;  // extra burn while sprinting
	inline constexpr float CarnivoreHungerMult  = 1.7f;  // carnivores burn faster (must keep hunting)
	inline constexpr float StarvationDamagePerSec = 6.f; // HP lost per second at zero hunger
	inline constexpr float StaminaDrainPerSec   = 12.f;  // stamina burned per second sprinting
	inline constexpr float StaminaRegenPerSec   = 8.f;   // stamina recovered per second cruising

	// --- Reproduction ---
	inline constexpr float MaturityAge          = 3.f;   // min seconds before breeding
	inline constexpr float ReproHungerFraction  = 0.72f; // fraction of max hunger required to breed
	inline constexpr float ReproHungerCost      = 0.45f; // fraction of max hunger spent per birth (each parent)
	inline constexpr float ReproCooldownBase     = 12.f; // seconds between births at ReproductionRate 0
	inline constexpr float ReproRateScale        = 0.6f; // higher ReproductionRate shortens the cooldown
	inline constexpr float MatingRadius          = 700.f;// how close a ready partner must be to breed

	// --- Genetic algorithm (offspring stat reallocation at birth) ---
	inline constexpr float ReallocStepsScale     = 30.f; // mutation steps = 1 + MutationRate * this
	inline constexpr float ReallocStepFraction   = 0.3f; // max share of movable budget shifted per step

	// --- Feeding / predation ---
	inline constexpr float EatReach             = 200.f; // contact distance to eat food or catch prey
	inline constexpr float FoodEnergy           = 35.f;  // hunger restored by one plant (before digestion)
	// Diet is a continuous gradient (0 = herbivore .. 1 = carnivore). It sets how efficiently a
	// boid digests each food type; below this floor it can't use that type at all (true specialists).
	inline constexpr float DietEfficiencyFloor   = 0.1f;

	// --- Flocking weights ---
	inline constexpr float SeparationRadius     = 350.f;
	inline constexpr float SeparationWeight     = 1.6f;
	inline constexpr float AlignmentWeight      = 1.0f;
	inline constexpr float CohesionWeightScale  = 0.06f; // multiplied by the Integration stat (lower = less clumping)
	inline constexpr float FleeWeight           = 4.5f;
	inline constexpr float ChaseWeightScale     = 0.5f;  // multiplied by the Aggressiveness stat
	inline constexpr float SeekFoodWeight       = 2.5f;  // herbivore pull toward food, scaled by hunger
	inline constexpr float SeekPartnerWeight	= 2.5f;
	inline constexpr float MaxSteerAccel        = 3000.f;
	inline constexpr float WanderAccel          = 600.f;

	// --- Water (land boids avoid it, and wade slowly if they enter) ---
	inline constexpr float WaterLookAhead       = 450.f;  // how far ahead a boid checks for water
	inline constexpr float WaterEdgeMargin      = 70.f;   // treat ground within this height of the sea as "shore"
	inline constexpr float WaterAvoidAccel      = 3200.f; // inland push for POOR swimmers approaching water
	inline constexpr float WaterEscapeAccel     = 2400.f; // ALWAYS-on push to shore once in water (no death traps)
	inline constexpr float WaterEscapeProbe     = 800.f;  // distance used to find the way to shore
	inline constexpr float WaterWadeSpeedMin    = 0.5f;   // swim speed of a non-swimmer (still able to get out)
	inline constexpr float WaterWadeSpeedMax    = 0.95f;  // swim speed of a strong swimmer (over water)
	// Aquatic adaptation in [0,1] from the genome (no dedicated gene, so it can evolve):
	// endurance (Stamina) helps, bulk (Biomass) hinders. Strong swimmers ignore water; weak ones avoid it.
	inline constexpr float SwimFromStamina      = 0.05f;
	inline constexpr float SwimFromBiomass      = 0.018f;
	inline constexpr float SwimBase             = 0.10f;

	// --- Movement smoothing & realistic perception ---
	inline constexpr float WanderDriftDegPerSec = 70.f;   // how fast the wander heading meanders (smooth, not jittery)
	inline constexpr float FacingInterpSpeed    = 7.f;    // how fast the body turns to face travel (higher = snappier)
	inline constexpr float PerceptionFOVDegrees = 230.f;  // forward field of view; things behind aren't perceived
	inline constexpr float ForceSmoothing       = 10.f;   // low-pass on steering force (reduces shake)

	// --- Debug draw (toggled with the B key) ---
	inline constexpr int32 DebugSampleStride    = 8;      // draw 1 in N boids, so the view isn't cluttered
	inline constexpr float DebugZLift           = 90.f;   // lift overlays above the body so they read clearly
	inline constexpr float DebugArrowThickness  = 4.f;
	inline constexpr float DebugCircleThickness = 2.5f;

	// --- Reproduction feedback (born-here flash + soft blip) ---
	inline constexpr float BirthFlashDuration   = 0.55f;  // seconds the spawn ring is shown
	inline constexpr float BirthFlashRadius      = 170.f; // ring expands to this radius
	inline constexpr float BirthSfxMinInterval   = 0.40f; // min seconds between blips (keeps it un-annoying)
	inline constexpr float BirthSfxVolume        = 0.13f; // soft

	// --- Terrain follow ---
	inline constexpr float GroundOffset          = 60.f;  // how far a boid floats above the surface
	inline constexpr float SlopeSpeedPenalty     = 0.6f;  // fraction of speed lost going straight uphill

	// --- Combat (predation) ---
	inline constexpr float IntimidationFleeScale = 0.15f; // prey flee harder from intimidating hunters
	inline constexpr float AttackStaminaCost     = 7.f;   // stamina spent per attack
	inline constexpr float AttackHungerCost      = 7.f;   // hunger spent per attack (hunting is metabolically costly)
	inline constexpr float AttackCooldownTime    = 1.3f;  // seconds between a boid's attacks
	inline constexpr float MinStaminaToAttack    = 6.f;   // can't attack below this stamina
	inline constexpr float HuntTierMargin        = 0.3f;  // only hunt prey this much lower on the carnivory scale
	inline constexpr float PackRadius            = 900.f; // allies within this distance count as a pack
	inline constexpr float PackDamagePerAlly     = 0.15f; // +damage per ally, multiplied by Integration
	inline constexpr int32 PackMaxAllies         = 6;     // diminishing returns cap
	inline constexpr float CounterDamageScale    = 1.1f;  // prey counter-hit = Damage*scale, gated by Aggressiveness
	inline constexpr float AdrenalineDuration    = 4.0f;  // seconds of flee burst after being hit
	inline constexpr float AdrenalineSpeedMult   = 1.9f;  // speed multiplier while adrenaline lasts

	// --- Carcasses (shared meat from a kill) ---
	inline constexpr float CarcassEnergyScale    = 14.f;  // carcass energy = prey Biomass * this (a meal, not a feast)
	inline constexpr float CarcassBite           = 22.f;  // energy a carnivore takes per feeding tick
	inline constexpr float CarcassDecayPerSec    = 6.f;   // carcasses rot away so predators can't hoard food
	inline constexpr float CarcassMeshScale      = 0.9f;  // render scale of a carcass

	// --- Food field ---
	inline constexpr int32 FoodTargetCount      = 3000;  // arena-wide plant population the sim maintains
	inline constexpr int32 FoodSpawnPerTick     = 30;    // max new plants per subsystem tick
	inline constexpr float FoodMeshScale        = 0.8f;  // render scale of a plant instance (grass tuft)

	// --- Creature appearance (every individual's genome shapes how it looks) ---
	// Colour = diet hue x vigour brightness; one instanced mesh per (hue, shade) bucket.
	inline constexpr int32 NumDietHues           = 10;
	inline constexpr int32 NumShades             = 3;
	inline constexpr int32 NumAppearanceBuckets  = NumDietHues * NumShades;
	inline constexpr float VigorNorm             = 45.f;  // normalises HP+Armor+Damage into [0,1]
	// Body shape: each axis reflects a different stat, so silhouettes vary per individual.
	inline constexpr float BodySizeBase          = 0.5f;  // baseline bulk
	inline constexpr float BodySizeFromHP        = 0.03f; // bulkier with HP
	inline constexpr float BodyStreamlineFromSpeed = 0.022f; // longer/leaner with run speed
	inline constexpr float BodyWidthFromArmor    = 0.03f; // broader with armour

	// Derived helpers ---------------------------------------------------------
	inline float MaxHP(const FBoidGenome& G)       { return FMath::Max(1.f, G.Get(EBoidStat::HP) * MaxHPScale); }
	inline float MaxStamina(const FBoidGenome& G)  { return FMath::Max(1.f, G.Get(EBoidStat::Stamina) * MaxStaminaScale); }
	inline float MaxHunger(const FBoidGenome& G)   { return FMath::Max(1.f, G.Get(EBoidStat::Hunger) * MaxHungerScale); }
	inline float WalkSpeed(const FBoidGenome& G)   { return FMath::Max(1.f, G.Get(EBoidStat::WalkSpeed) * WalkSpeedScale); }
	inline float RunSpeed(const FBoidGenome& G)    { return FMath::Max(WalkSpeed(G), G.Get(EBoidStat::RunSpeed) * RunSpeedScale); }
	inline float PerceptionRadius(const FBoidGenome& G) { return FMath::Max(100.f, G.Get(EBoidStat::Perception) * PerceptionScale); }
	inline float Lifespan(const FBoidGenome& G)    { return FMath::Max(5.f, G.Get(EBoidStat::Lifespan) * LifespanScale); }
	inline float ReproCooldown(const FBoidGenome& G)
	{
		return ReproCooldownBase / (1.f + FMath::Max(0.f, G.Get(EBoidStat::ReproductionRate)) * ReproRateScale);
	}
	// --- Diet as a continuous gradient ---------------------------------------
	// Efficiency in [0,1] of extracting energy from each food type, crossfading with Diet.
	inline float PlantDigestionFromDiet(float Diet) { return FMath::Clamp(1.f - Diet, 0.f, 1.f); }
	inline float MeatDigestionFromDiet(float Diet)  { return FMath::Clamp(Diet, 0.f, 1.f); }
	inline float PlantDigestion(const FBoidGenome& G) { return PlantDigestionFromDiet(G.Get(EBoidStat::Diet)); }
	inline float MeatDigestion(const FBoidGenome& G)  { return MeatDigestionFromDiet(G.Get(EBoidStat::Diet)); }

	// Whether it's worth bothering with a food type at all (specialists ignore the other one).
	inline bool CanEatPlants(const FBoidGenome& G) { return PlantDigestion(G) > DietEfficiencyFloor; }
	inline bool CanHunt(const FBoidGenome& G)      { return MeatDigestion(G) > DietEfficiencyFloor; }

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

	inline int32 HueBucket(float Diet)  { return FMath::Clamp(static_cast<int32>(FMath::Clamp(Diet, 0.f, 1.f) * NumDietHues), 0, NumDietHues - 1); }
	inline int32 ShadeBucket(float V)   { return FMath::Clamp(static_cast<int32>(V * NumShades), 0, NumShades - 1); }

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
}
