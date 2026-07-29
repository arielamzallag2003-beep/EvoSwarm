#pragma once

#include "CoreMinimal.h"
#include "BehaviourTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Behaviour type registry
//  Replaces every class XxxBehaviour : IBehaviour
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EBehaviourType : uint8
{
    Alignment         UMETA(DisplayName = "Alignment"),
    Cohesion          UMETA(DisplayName = "Cohesion"),
    Separation        UMETA(DisplayName = "Separation"),
    Seek              UMETA(DisplayName = "Seek"),
    Arrival           UMETA(DisplayName = "Arrival"),
    Flee              UMETA(DisplayName = "Flee"),
    Wander            UMETA(DisplayName = "Wander"),
    Pursuit           UMETA(DisplayName = "Pursuit"),
    StayInRadius      UMETA(DisplayName = "Stay In Radius"),
    ObstacleAvoidance UMETA(DisplayName = "Obstacle Avoidance"),
    Formation         UMETA(DisplayName = "Formation"),
    Command           UMETA(DisplayName = "Command"),
};

// ─────────────────────────────────────────────────────────────────────────────
//  FBehaviourParams
//  Replaces the per-class fields of each IBehaviour implementation.
//  Param0-2 are typed by convention (see comments per behaviour type).
//
//  Alignment:         — (no extra params)
//  Cohesion:          — (no extra params)
//  Separation:        Param0 = SeparationRadius (default 2)
//  Seek:              — (no extra params)
//  Arrival:           Param0 = SlowingRadius (default 5)
//  Flee:              Param0 = PanicDistance (default 5)
//  Wander:            Param0 = Jitter, Param1 = Radius, Param2 = Distance
//  Pursuit:           Param0 = MaxLookAheadTime (default 2)
//  StayInRadius:      Param0 = BoundaryRadius (default 20)
//  ObstacleAvoidance: Param0 = AvoidanceRadius, Param1 = LookAheadDist
//  Formation:         Param0 = ArrivalRadius (default 1)
//  Command:           Param0 = ArrivalRadius (default 1)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FBehaviourParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EBehaviourType Type = EBehaviourType::Alignment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float Weight = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Param0 = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Param1 = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Param2 = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  FBehaviourEntry
//  One slot in a sorted behaviour stack (sorted descending by Priority).
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FBehaviourEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FBehaviourParams Params;
};

/** Sort predicate: highest Priority first. */
struct FBehaviourPrioritySorter
{
    FORCEINLINE bool operator()(const FBehaviourEntry& A, const FBehaviourEntry& B) const
    {
        return A.Params.Priority > B.Params.Priority;
    }
};
