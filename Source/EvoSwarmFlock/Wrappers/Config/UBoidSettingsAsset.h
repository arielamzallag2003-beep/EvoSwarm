#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Flock/FlockTypes.h"
#include "UBoidSettingsAsset.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UBoidSettingsAsset
//
//  Replaces: BoidProfile.cs (ScriptableObject, IBoidSettings)
//
//  A UDataAsset that wraps FBoidSettings so designers can author boid
//  tuning in the Content Browser and reference it from UFlockManagerComponent.
//  Call ToStruct() to push the values into FFlock::SettingsTemplates.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class EVOSWARMFLOCK_API UBoidSettingsAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // ── Movement ─────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.1"))
    float MaxSpeed = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.1"))
    float MaxForce = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01"))
    float Mass = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Drag = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
    EMovementPlane MovementPlane = EMovementPlane::XZ;

    // ── Perception ────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "0.1"))
    float PerceptionRadius = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "1.0", ClampMax = "360.0"))
    float FieldOfViewAngle = 270.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception", meta = (ClampMin = "1"))
    int32 MaxNeighbors = 12;

    // ── Avoidance ─────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Avoidance", meta = (ClampMin = "0.1"))
    float ObstacleAvoidanceDistance = 4.f;

    // ── Conversion ────────────────────────────────────────────────────────────
    /** Build an FBoidSettings struct to push into FFlock::SettingsTemplates. */
    UFUNCTION(BlueprintPure, Category = "EvoSwarm")
    FBoidSettings ToStruct() const
    {
        FBoidSettings S;
        S.MaxSpeed                 = MaxSpeed;
        S.MaxForce                 = MaxForce;
        S.Mass                     = Mass;
        S.Drag                     = Drag;
        S.MovementPlane            = MovementPlane;
        S.PerceptionRadius         = PerceptionRadius;
        S.FieldOfViewAngle         = FieldOfViewAngle;
        S.MaxNeighbors             = MaxNeighbors;
        S.ObstacleAvoidanceDistance = ObstacleAvoidanceDistance;
        return S;
    }
};
