// Copyright Evoswarm.
//
// The behaviour systems. Each runs once per frame in the PrePhysics phase, chained via
// ExecuteAfter into the order: GridUpdate -> Steering -> Movement -> Feeding ->
// Metabolism -> Reproduction -> Render. They only read/write fragments and the grid;
// all structural changes (births) are queued to the sim subsystem, deaths are deferred.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "BoidProcessors.generated.h"

/** Rebuilds the spatial hash each frame from current boid + food positions. Runs first. */
UCLASS()
class EVOSWARM_API UBoidGridUpdateProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UBoidGridUpdateProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery BoidQuery;
	FMassEntityQuery FoodQuery;
};

/** Boids steering: separation + alignment + cohesion, plus predator/prey forces. */
UCLASS()
class EVOSWARM_API UBoidSteeringProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UBoidSteeringProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
	FRandomStream Rng = FRandomStream(91);
};

/** Integrates steering force into velocity + transform, applies bounds and stamina. */
UCLASS()
class EVOSWARM_API UBoidMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UBoidMovementProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
};

/** Herbivores eat nearby food; carnivores catch and consume prey for biomass. */
UCLASS()
class EVOSWARM_API UBoidFeedingProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UBoidFeedingProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
};

/** Ages boids, drains hunger, regenerates HP, and kills the starved / old (deferred). */
UCLASS()
class EVOSWARM_API UBoidMetabolismProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UBoidMetabolismProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
};

/** Well-fed, mature boids queue mutated offspring with the sim subsystem. Closes the loop. */
UCLASS()
class EVOSWARM_API UBoidReproductionProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UBoidReproductionProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
	FRandomStream Rng = FRandomStream(4242);
};

/** Optional, key-toggled: draws heading arrows + perception circles for a sparse sample of boids. */
UCLASS()
class EVOSWARM_API UBoidDebugDrawProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UBoidDebugDrawProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
};

/** Rots carcasses over time so meat can't be hoarded (predator starvation pressure). */
UCLASS()
class EVOSWARM_API UFoodDecayProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UFoodDecayProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
};

/** Aggregates per-species population + average genome values each frame for the HUD. */
UCLASS()
class EVOSWARM_API UBoidStatsProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UBoidStatsProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
};

/** Renders boids into diet-colour buckets (colour from diet, size from genome) and food/carcasses. */
UCLASS()
class EVOSWARM_API UBoidRenderProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UBoidRenderProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	FMassEntityQuery EntityQuery;
	FMassEntityQuery FoodQuery;

	/** Reused per frame: index = diet-colour bucket, value = that bucket's instance transforms. */
	TArray<TArray<FTransform>> PerBucketTransforms;

	/** Reused per frame: plant and carcass instance transforms. */
	TArray<FTransform> FoodTransforms;
	TArray<FTransform> CarcassTransforms;
};
