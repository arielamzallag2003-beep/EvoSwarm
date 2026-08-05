// Copyright Evoswarm.
//
// A uniform 2D spatial hash, rebuilt each frame, that lets flocking / feeding /
// predation answer "who is near me?" cheaply instead of O(n^2) scans. It also brokers
// single-claim consumption so two predators can't eat the same prey in one frame.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityHandle.h" // Latest include order no longer pulls this in via MassEntityTypes.h
#include "BoidFragments.h"    // EFoodType
#include "BoidGridSubsystem.generated.h"

/** A boid snapshot stored in the grid for this frame. */
struct FGridAgent
{
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FMassEntityHandle Entity;
	int32 SpeciesIndex = 0;
	float Biomass = 0.f;
	float Stealth = 0.f;
	float Diet = 0.f;
	float Intimidation = 0.f;
	bool bCanMate = false; // mature, fed, off cooldown -> available as a breeding partner
	float Attractiveness = 0.f; // Attractiveness in [0,1] (see ComputeAttractivenessScore) -> mate-choice fitness signal
};

/** A food node snapshot stored in the grid for this frame. */
struct FGridFood
{
	FVector Position = FVector::ZeroVector;
	FMassEntityHandle Entity;
	float Energy = 0.f;
	EFoodType Type = EFoodType::Plant;
};

UCLASS()
class EVOSWARM_API UBoidGridSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Clear all per-frame data. Called by the grid-update processor before population. */
	void BeginFrame();

	void AddAgent(const FGridAgent& Agent);
	void AddFood(const FGridFood& Food);

	/** Visit every agent whose position is within Radius of Center. */
	void QueryAgents(const FVector& Center, float Radius, TFunctionRef<void(const FGridAgent&)> Visitor) const;

	/** Find the closest un-claimed food of the given type within Radius. Returns false if none. */
	bool FindNearestFood(const FVector& Center, float Radius, EFoodType Type, FGridFood& OutFood) const;

	/** Atomically claim an entity for consumption this frame. Returns false if already claimed. */
	bool TryClaim(FMassEntityHandle Entity);

	/** World units per grid cell. Roughly the largest query radius for best performance. */
	float CellSize = 600.f;

private:
	FIntPoint CellOf(const FVector& Location) const;
	static uint64 KeyOf(FMassEntityHandle Entity);

	TArray<FGridAgent> Agents;
	TArray<FGridFood> Foods;
	TMap<FIntPoint, TArray<int32>> AgentCells;
	TMap<FIntPoint, TArray<int32>> FoodCells;
	TSet<uint64> ClaimedThisFrame;
};
