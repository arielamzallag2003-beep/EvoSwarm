#include "Formations/FormationSystem.h"
#include "Flock/FlockTypes.h"
#include "Flock/FlockContainer.h"
#include "Math/UnrealMathUtility.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Slot geometry helpers (replaces IFormation implementations)
// ─────────────────────────────────────────────────────────────────────────────

static FVector LineFormation_SlotOffset(int32 SlotIdx, int32 TotalUnits, float Spacing)
{
    float Center = (TotalUnits - 1) * Spacing / 2.f;
    return FVector(SlotIdx * Spacing - Center, 0.f, 0.f);
}

static FVector CircleFormation_SlotOffset(int32 SlotIdx, int32 TotalUnits, float Spacing)
{
    int32 N      = FMath::Max(1, TotalUnits);
    float Angle  = (2.f * PI * SlotIdx) / N;
    float Radius = (TotalUnits * Spacing) / (2.f * PI);
    return FVector(FMath::Cos(Angle) * Radius, 0.f, FMath::Sin(Angle) * Radius);
}

static FVector WedgeFormation_SlotOffset(int32 SlotIdx, int32 /*TotalUnits*/, float Spacing)
{
    int32 Row = FMath::FloorToInt(FMath::Sqrt(static_cast<float>(SlotIdx + 1)));
    int32 Col = SlotIdx - Row * Row;
    float X   = static_cast<float>(Col - Row) * Spacing;
    float Z   = static_cast<float>(-Row) * Spacing;
    return FVector(X, 0.f, Z);
}

// Transform a local-space offset into world space using leader position + forward.
// Replaces IFormation::GetWorldPosition.
static FVector LocalToWorld(const FVector& LocalOffset,
                             const FVector& LeaderPos,
                             const FVector& LeaderFwd)
{
    FVector SafeFwd = LeaderFwd.IsNearlyZero() ? FVector::ForwardVector : LeaderFwd.GetSafeNormal();
    FQuat   Rot     = FQuat::FindBetweenNormals(FVector::ForwardVector, SafeFwd);
    return LeaderPos + Rot.RotateVector(LocalOffset);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RebuildFormationSlots
//  Replaces IFormationController::AssignSlots + ReassignSlots
// ─────────────────────────────────────────────────────────────────────────────
void RebuildFormationSlots(FFormationState& Formation, TArray<FBoidData>& Boids)
{
    Formation.SlotAssignments.Reset();
    Formation.SlotPositions.Reset();

    if (!Formation.bIsActive
        || Formation.Type == EFormationType::None
        || Formation.LeaderIndex == INDEX_NONE
        || !Boids.IsValidIndex(Formation.LeaderIndex))
    {
        // Clear formation flag on all boids
        for (FBoidData& B : Boids) B.bInFormation = false;
        Formation.bSlotsDirty = false;
        return;
    }

    const FBoidData& Leader = Boids[Formation.LeaderIndex];

    // Collect follower indices (every active boid that isn't the leader)
    TArray<int32> Followers;
    for (int32 i = 0; i < Boids.Num(); ++i)
    {
        if (i == Formation.LeaderIndex || !Boids[i].bIsActive) continue;
        Followers.Add(i);
    }

    int32 NumSlots = Followers.Num();
    Formation.SlotAssignments.SetNum(NumSlots);
    Formation.SlotPositions.SetNum(NumSlots);

    for (int32 s = 0; s < NumSlots; ++s)
    {
        FVector LocalOff;
        switch (Formation.Type)
        {
        case EFormationType::Line:
            LocalOff = LineFormation_SlotOffset(s, NumSlots, Formation.Spacing);
            break;
        case EFormationType::Circle:
            LocalOff = CircleFormation_SlotOffset(s, NumSlots, Formation.Spacing);
            break;
        case EFormationType::Wedge:
            LocalOff = WedgeFormation_SlotOffset(s, NumSlots, Formation.Spacing);
            break;
        default:
            LocalOff = FVector::ZeroVector;
        }

        FVector WorldPos = LocalToWorld(LocalOff, Leader.Position, Leader.Forward);

        int32 BoidIdx = Followers[s];
        Formation.SlotAssignments[s]  = BoidIdx;
        Formation.SlotPositions[s]    = WorldPos;

        Boids[BoidIdx].bInFormation   = true;
        Boids[BoidIdx].FormationSlot  = WorldPos;
        Boids[BoidIdx].LeaderBoidIndex = Formation.LeaderIndex;
    }

    // Leader itself is not in formation
    Boids[Formation.LeaderIndex].bInFormation = false;

    Formation.bSlotsDirty = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  UpdateFormationSlotPositions
//  Fast per-tick update: does NOT reassign boid ↔ slot, only recalculates the
//  world positions based on the leader's current position/forward.
//  Call this every tick when the formation is active to keep slots moving with
//  the leader. Only call RebuildFormationSlots when the assignment changes.
// ─────────────────────────────────────────────────────────────────────────────
void UpdateFormationSlotPositions(FFormationState& Formation, TArray<FBoidData>& Boids)
{
    if (!Formation.bIsActive
        || Formation.Type == EFormationType::None
        || Formation.LeaderIndex == INDEX_NONE
        || !Boids.IsValidIndex(Formation.LeaderIndex))
        return;

    const FBoidData& Leader   = Boids[Formation.LeaderIndex];
    int32            NumSlots  = Formation.SlotAssignments.Num();

    for (int32 s = 0; s < NumSlots; ++s)
    {
        FVector LocalOff;
        switch (Formation.Type)
        {
        case EFormationType::Line:
            LocalOff = LineFormation_SlotOffset(s, NumSlots, Formation.Spacing);
            break;
        case EFormationType::Circle:
            LocalOff = CircleFormation_SlotOffset(s, NumSlots, Formation.Spacing);
            break;
        case EFormationType::Wedge:
            LocalOff = WedgeFormation_SlotOffset(s, NumSlots, Formation.Spacing);
            break;
        default:
            LocalOff = FVector::ZeroVector;
        }

        FVector WorldPos = LocalToWorld(LocalOff, Leader.Position, Leader.Forward);
        Formation.SlotPositions[s] = WorldPos;

        int32 BoidIdx = Formation.SlotAssignments[s];
        if (Boids.IsValidIndex(BoidIdx))
            Boids[BoidIdx].FormationSlot = WorldPos;
    }
}
