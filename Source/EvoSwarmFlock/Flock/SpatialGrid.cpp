#include "Flock/SpatialGrid.h"

void FSpatialGrid::Rebuild(const TArray<FBoidData>& Boids, float CellOverride)
{
    if (CellOverride > 0.f) CellSize = CellOverride;

    // Clear previous frame's data without de-allocating bucket arrays
    for (auto& Pair : Cells) Pair.Value.Reset();
    Cells.Reset();

    // Insert every active boid
    for (int32 i = 0; i < Boids.Num(); ++i)
    {
        const FBoidData& B = Boids[i];
        if (!B.bIsActive) continue;

        FIntVector Cell = WorldToCell(B.Position);
        Cells.FindOrAdd(Cell).Add(i);
    }
}

void FSpatialGrid::QueryNeighbors(
    const TArray<FBoidData>& Boids,
    int32  BoidIndex,
    float  PerceptionRadius,
    int32  MaxCount,
    TArray<int32>& OutIndices) const
{
    OutIndices.Reset();

    if (!Boids.IsValidIndex(BoidIndex)) return;

    const FBoidData& Self   = Boids[BoidIndex];
    const float      RadSq  = PerceptionRadius * PerceptionRadius;

    // Determine cell range to check
    int32 StepX = FMath::CeilToInt(PerceptionRadius / CellSize);
    int32 StepY = StepX;
    int32 StepZ = StepX;

    FIntVector CenterCell = WorldToCell(Self.Position);

    for (int32 dz = -StepZ; dz <= StepZ; ++dz)
    for (int32 dy = -StepY; dy <= StepY; ++dy)
    for (int32 dx = -StepX; dx <= StepX; ++dx)
    {
        FIntVector TestCell(CenterCell.X + dx, CenterCell.Y + dy, CenterCell.Z + dz);
        const TArray<int32>* CellBoids = Cells.Find(TestCell);
        if (!CellBoids) continue;

        for (int32 Idx : *CellBoids)
        {
            if (Idx == BoidIndex) continue;
            if (!Boids[Idx].bIsActive) continue;

            float SqDist = FVector::DistSquared(Boids[Idx].Position, Self.Position);
            if (SqDist < RadSq)
            {
                OutIndices.Add(Idx);
                if (OutIndices.Num() >= MaxCount) return;
            }
        }
    }
}
