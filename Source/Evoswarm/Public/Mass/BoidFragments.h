// Copyright Evoswarm.
//
// The per-entity data (fragments), per-species shared data, and archetype tags that
// define what a boid (and a food node) IS in Mass terms. Behaviour lives in processors;
// this file is pure data.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "BoidStats.h"
#include "BoidFragments.generated.h"

class USpeciesConfig;

/** The inheritable genome carried by every boid. */
USTRUCT()
struct EVOSWARM_API FBoidGenomeFragment : public FMassFragment
{
	GENERATED_BODY()

	FBoidGenome Genome;
};

/**
 * Coarse behaviour state. Decided once per frame by the steering processor and read by
 * everything downstream (movement, feeding, metabolism, debug draw). Priority order is
 * danger > exhaustion > breeding > feeding, so only one state can be active at a time.
 */
UENUM()
enum class EBoidState : uint8
{
	Wandering,   // default: meandering and flocking, no pressing need
	Foraging,    // hungry, steering toward food
	Hunting,     // hungry carnivore closing on live prey
	Fleeing,     // a higher-tier predator is in range, or recently took a hit
	Mating,      // ready to breed and steering toward a chosen partner
	Sleeping     // resting: does not move or feed, burns little, heals fast
};

/** Per-frame mutable life state. */
USTRUCT()
struct EVOSWARM_API FBoidStateFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Current hit points (dies at <= 0). Initialised from the HP stat. */
	float CurrentHP = 1.f;

	/** Current stamina; spent while sprinting, regenerates while calm. */
	float CurrentStamina = 1.f;

	/** Stored food. Drains over time; starvation begins at 0. Refilled by eating. */
	float CurrentHunger = 1.f;

	/** Seconds lived. Death of old age past the Lifespan stat. */
	float Age = 0.f;

	/** Seconds until this boid can reproduce again. */
	float ReproCooldown = 0.f;

	/** Seconds until this boid can attack again (stamina-gated combat). */
	float AttackCooldown = 0.f;

	/** Seconds of adrenaline remaining (recently attacked -> short flee speed burst). */
	float Adrenaline = 0.f;

	/** Persistent wander heading (radians); drifts slowly for smooth, non-jittery meandering. */
	float WanderAngle = 0.f;

	/**
	 * Locomotion cycle phase in [0, 2*PI). Advanced by DISTANCE TRAVELLED (not by elapsed time)
	 * in the movement processor, so the stride rate follows ground speed and the feet don't
	 * skate during acceleration. Consumed by the render processor for the procedural gait.
	 * Purely cosmetic: no gameplay processor reads it back.
	 */
	float GaitPhase = 0.f;

	/** Generations from the founding population (0 = initial spawn). Offspring = parent + 1. */
	int32 Generation = 0;

	// Nombre de fois ou l'individu s'est reproduit
	int32 ReproductionCount = 0;

	/** Current behaviour. Written by the steering processor, read by everyone else. */
	EBoidState CurrentBehaviorState = EBoidState::Wandering;
};

/** What kind of food an entity is. */
UENUM()
enum class EFoodType : uint8
{
	Plant,   // grows in the world; eaten by herbivores/omnivores
	Carcass  // dropped by a kill; eaten by carnivores/omnivores (scavenging)
};

/**
 * Shared across all boids of one species. Holds the species index (selects the render
 * ISM), tint, and a pointer back to the authored config used by reproduction/mutation.
 */
USTRUCT()
struct EVOSWARM_API FBoidSpeciesSharedFragment : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SpeciesIndex = 0;

	UPROPERTY()
	FLinearColor Color = FLinearColor::White;

	/** Uniform render scale applied to each instance of this species. */
	UPROPERTY()
	float MeshScale = 1.f;

	UPROPERTY()
	TObjectPtr<USpeciesConfig> Config = nullptr;
};

/** A consumable food node (plant / energy source). */
USTRUCT()
struct EVOSWARM_API FFoodFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Remaining nutritional energy. Consumed to refill a feeder's hunger. */
	float Energy = 1.f;

	/** Plant or carcass — determines who can eat it (via diet digestion). */
	EFoodType Type = EFoodType::Plant;
};

/** Marks an entity as a boid. */
USTRUCT()
struct EVOSWARM_API FBoidTag : public FMassTag
{
	GENERATED_BODY()
};

/** Marks an entity as a food node. */
USTRUCT()
struct EVOSWARM_API FFoodTag : public FMassTag
{
	GENERATED_BODY()
};