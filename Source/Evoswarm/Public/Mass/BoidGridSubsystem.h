// Copyright Evoswarm.
//
// A uniform 2D spatial grid that answers "who is near me?" in ~O(1) per boid instead of
// O(n^2). It is a DENSE grid: cells live in a flat array indexed by (iy*Dim + ix), so a
// cell lookup is pointer arithmetic with no hashing. The arena is bounded, so the whole
// grid is a fixed-size array sized once from Evo::ArenaHalfExtent / CellSize.
//
// Cost model for one radius-R query at cell size c and boid density rho:
//   cells scanned  = (2*ceil(R/c) + 1)^2      (each a direct array access)
//   boids examined ~= rho * (2R + c)^2
// Making the cells a flat array removes the hash constant from both the per-frame rebuild
// (thousands of inserts) and every query.
//
// It also brokers single-claim consumption so two predators can't eat the same prey in one frame.

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
	float CellSize = 1000.f;

private:
	static uint64 KeyOf(FMassEntityHandle Entity);

	/** Size the dense grid from Evo::ArenaHalfExtent / CellSize (idempotent; called lazily). */
	void EnsureGrid();
	/** Clamped integer cell coordinates for a world position. */
	void CellCoord(const FVector& P, int32& OutX, int32& OutY) const;
	/** Flat cell index for a world position (clamped into the grid). */
	int32 CellIndex(const FVector& P) const;

	int32 GridDim = 0;        // cells per side; 0 until EnsureGrid runs
	float GridOrigin = 0.f;   // world coordinate of the grid's min edge
	float GridInvCell = 0.f;  // 1 / CellSize

	TArray<FGridAgent> Agents;
	TArray<FGridFood> Foods;

	// Flat cell buckets, size GridDim*GridDim. Touched* lists let BeginFrame reset only the
	// cells that were actually used (keeping their allocations), so the clear is O(occupied).
	TArray<TArray<int32>> AgentCells;
	TArray<int32> TouchedAgentCells;
	/** One dense grid per EFoodType, so carcass queries never wade through thousands of plants. */
	TArray<TArray<int32>> FoodCells[2];
	TArray<int32> TouchedFoodCells[2];

	TSet<uint64> ClaimedThisFrame;
};
