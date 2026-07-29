#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Behaviours/BehaviourTypes.h"
#include "UBehaviourStackAsset.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UBehaviourStackAsset
//
//  Replaces: BehaviourAsset.cs (abstract ScriptableObject)
//            AlignmentAsset, CohesionAsset, SeparationAsset, SeekAsset,
//            ArrivalAsset, FleeAsset, WanderAsset, PursuitAsset,
//            StayInRadiusAsset  — all individual ScriptableObjects.
//
//  Instead of one asset per behaviour type, designers author a sorted list
//  of FBehaviourEntry inline in the Details panel.
//  Call BuildStack() to get the sorted array for FFlock::DefaultBehaviours
//  or FStateDefinition::Behaviours.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class EVOSWARMFLOCK_API UBehaviourStackAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    /**
     * Ordered behaviour stack.
     * Each entry specifies the behaviour Type, Weight, Priority, and any
     * type-specific parameters (Param0/1/2 — see BehaviourTypes.h comment block
     * for the meaning per EBehaviourType).
     * The array is re-sorted descending by Priority at runtime before use.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Behaviours")
    TArray<FBehaviourEntry> Behaviours;

    /**
     * Return the behaviour stack sorted descending by Priority.
     * Safe to call every frame — returns a copy; does not mutate the asset.
     */
    UFUNCTION(BlueprintPure, Category = "EvoSwarm")
    TArray<FBehaviourEntry> BuildStack() const
    {
        TArray<FBehaviourEntry> Result = Behaviours;
        Result.Sort([](const FBehaviourEntry& A, const FBehaviourEntry& B)
        {
            return A.Params.Priority > B.Params.Priority;
        });
        return Result;
    }
};
