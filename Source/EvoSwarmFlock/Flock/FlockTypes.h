#pragma once

#include "CoreMinimal.h"
#include "FlockTypes.generated.h"

// ─────────────────────────────────────────────
//  Movement plane constraint
// ─────────────────────────────────────────────
UENUM(BlueprintType)
enum class EMovementPlane : uint8
{
    XZ   UMETA(DisplayName = "XZ (Horizontal)"),
    XY   UMETA(DisplayName = "XY (2D)"),
    Full UMETA(DisplayName = "Full 3D"),
};

// ─────────────────────────────────────────────
//  Per-boid tuning data (shared across species)
//  Replaces IBoidSettings
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FBoidSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MaxSpeed = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MaxForce = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float Mass = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float Drag = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    EMovementPlane MovementPlane = EMovementPlane::XZ;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
    float PerceptionRadius = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
    float FieldOfViewAngle = 270.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
    int32 MaxNeighbors = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance")
    float ObstacleAvoidanceDistance = 4.f;
};

// ─────────────────────────────────────────────
//  Hot boid simulation data — lives in a
//  contiguous TArray<FBoidData>.
//  Replaces IBoid (interface + heap object).
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FBoidData
{
    GENERATED_BODY()

    // ── Transform ──────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Transform")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Transform")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Transform")
    FVector Forward  = FVector::ForwardVector;

    // ── Force accumulation ──────────────────────
    FVector AccumulatedForce = FVector::ZeroVector;

    // ── Settings ────────────────────────────────
    // Index into FFlock::SettingsTemplates
    int32   SettingsIndex = 0;

    // ── State machine ───────────────────────────
    uint8   StateIndex = 0;
    uint8   PrevState  = 0;

    // ── Wander (replaces WanderBehaviour fields) ─
    float   WanderAngle = 0.f;  // radians, updated each tick
    uint32  RandSeed    = 12345u;

    // ── Flags (bit-packed) ──────────────────────
    uint8   bIsActive    : 1;
    uint8   bInFormation : 1;
    uint8   bIsSprinting : 1;   // set by Pass_UpdateStats when stamina > 0 and speed > threshold
    uint8   FlagPad      : 5;

    // ── Targeting ───────────────────────────────
    // INDEX_NONE (-1) = no target
    int32  TargetBoidIndex = INDEX_NONE;
    FVector SeekTarget     = FVector::ZeroVector;
    bool    bHasSeekTarget = false;

    // ── Formation ───────────────────────────────
    FVector FormationSlot   = FVector::ZeroVector;
    int32   LeaderBoidIndex = INDEX_NONE;
    FVector CommandTarget   = FVector::ZeroVector;
    bool    bHasCommandTarget = false;

    FBoidData()
        : bIsActive(1), bInFormation(0), bIsSprinting(0), FlagPad(0)
    {}
};


// ─────────────────────────────────────────────
//  Obstacle — plain data, replaces IObstacle
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FObstacleData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite)
    float   Radius   = 1.f;

    UPROPERTY(BlueprintReadWrite)
    bool    bIsActive = true;
};

// ─────────────────────────────────────────────
//  Threat (position of a threatening entity)
// ─────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FThreatData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;
};

// ─────────────────────────────────────────────
//  Lightweight context passed to every behaviour
//  free function — stack-allocated, no heap.
//  Replaces IBoidContext (heap class).
// ─────────────────────────────────────────────
struct FBoidContext
{
    // Pointers into the owning flock arrays — NOT owned
    const FBoidData*       Self         = nullptr;
    const FBoidSettings*   Settings     = nullptr;
    const TArray<int32>*   NeighborIdx  = nullptr;   // indices into Flock.Boids
    const TArray<FBoidData>* AllBoids   = nullptr;
    const TArray<FObstacleData>* Obstacles = nullptr;
    const TArray<FThreatData>*   Threats   = nullptr;

    float  DeltaTime  = 0.f;
    float  TotalTime  = 0.f;

    // Flock-level anchor (for StayInRadius)
    FVector AnchorPosition = FVector::ZeroVector;
};
