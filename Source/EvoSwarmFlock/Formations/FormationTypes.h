#pragma once

#include "CoreMinimal.h"
#include "FormationTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FormationTypes.h
//
//  Replaces IFormation / IFormationController (heap objects, Dictionary slots).
//  DOD approach:
//    - Pre-compute world-space slot positions into FFormationState::SlotPositions
//    - Boids read FBoidData::FormationSlot; no dictionary lookup
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EFormationType : uint8
{
    None   UMETA(DisplayName = "None"),
    Line   UMETA(DisplayName = "Line"),
    Circle UMETA(DisplayName = "Circle"),
    Wedge  UMETA(DisplayName = "Wedge"),
};

USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FFormationState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFormationType Type = EFormationType::None;

    /** Index of the leader boid in FFlock::Boids. INDEX_NONE = no leader. */
    UPROPERTY(BlueprintReadWrite)
    int32 LeaderIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
    float Spacing = 2.f;

    UPROPERTY(BlueprintReadWrite)
    bool bIsActive = false;

    /**
     * Cached world-space slot positions, one per participating boid.
     * Rebuilt whenever LeaderIndex, Type, Spacing, or boid count changes.
     * Set bSlotsDirty = true to trigger a rebuild on next tick.
     */
    TArray<FVector> SlotPositions;

    /**
     * Mapping: SlotAssignments[slot_index] = boid_index.
     * Rebuilt alongside SlotPositions.
     */
    TArray<int32> SlotAssignments;

    bool bSlotsDirty = true;
};
