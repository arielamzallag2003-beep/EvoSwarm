#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Formations/FormationTypes.h"
#include "Flock/FlockContainer.h"
#include "UFormationSetupAsset.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UFormationSetupAsset
//
//  Replaces: FormationAsset.cs (ScriptableObject — creates IFormation  +
//            IFormationController.Spacing)
//
//  Holds the formation type and slot spacing.
//  Call ApplyToFlock() to configure FFlock::Formation data.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class EVOSWARMFLOCK_API UFormationSetupAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation")
    EFormationType Type = EFormationType::Line;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Formation", meta = (ClampMin = "0.1"))
    float Spacing = 2.f;

    /**
     * Write formation config into the flock.
     * @param LeaderIndex  Index of the leader boid in FFlock::Boids.
     *                     Pass INDEX_NONE to keep the current leader.
     */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void ApplyToFlock(UPARAM(ref) FFlock& Flock, int32 LeaderIndex) const
    {
        Flock.Formation.Type       = Type;
        Flock.Formation.Spacing    = Spacing;
        Flock.Formation.bIsActive  = (Type != EFormationType::None);
        Flock.Formation.bSlotsDirty = true;

        if (LeaderIndex != INDEX_NONE)
            Flock.Formation.LeaderIndex = LeaderIndex;
    }
};
