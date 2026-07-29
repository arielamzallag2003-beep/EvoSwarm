#pragma once

#include "CoreMinimal.h"
#include "Flock/FlockTypes.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FSpatialGrid
//
//  Replaces: LINQ neighbor queries in Flock.GetNeighbors()
//    _boids.Where(...).Where(...).Take(n).ToList()  ← 3 heap allocs per boid
//
//  DOD approach:
//    - Uniform grid rebuilt once per tick in Pass 1.
//    - QueryNeighbors fills a pre-allocated TArray<int32> (boid indices).
//    - No heap allocation inside the simulation hot path.
// ─────────────────────────────────────────────────────────────────────────────
struct EVOSWARMFLOCK_API FSpatialGrid
{
public:
    explicit FSpatialGrid(float InCellSize = 10.f) : CellSize(InCellSize) {}

    // ── Build ────────────────────────────────────────────────────────────────
    /**
     * Clear and repopulate the grid from the current boid positions.
     * Call once per tick before any neighbour queries.
     * @param Boids        The flat boid array from FFlock.
     * @param CellOverride Pass > 0 to override the stored CellSize.
     */
    void Rebuild(const TArray<FBoidData>& Boids, float CellOverride = 0.f);

    // ── Query ────────────────────────────────────────────────────────────────
    /**
     * Fill OutIndices with the indices (into the original Boids array) of all
     * active boids within PerceptionRadius of BoidIndex, up to MaxCount.
     * The boid itself is never included.
     */
    void QueryNeighbors(
        const TArray<FBoidData>& Boids,
        int32  BoidIndex,
        float  PerceptionRadius,
        int32  MaxCount,
        TArray<int32>& OutIndices) const;

    float CellSize;

private:
    // Cell coordinate → list of boid indices in that cell
    TMap<FIntVector, TArray<int32>> Cells;

    FORCEINLINE FIntVector WorldToCell(const FVector& Pos) const
    {
        return FIntVector(
            FMath::FloorToInt(Pos.X / CellSize),
            FMath::FloorToInt(Pos.Y / CellSize),
            FMath::FloorToInt(Pos.Z / CellSize)
        );
    }
};
