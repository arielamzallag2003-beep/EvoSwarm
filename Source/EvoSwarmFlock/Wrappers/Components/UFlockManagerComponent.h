#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Flock/FlockContainer.h"
#include "Flock/FlockSubsystem.h"
#include "Actor/ABoidActor.h"
#include "Wrappers/Config/UBoidSettingsAsset.h"
#include "Wrappers/Config/UBehaviourStackAsset.h"
#include "Wrappers/Config/UStateMachineSetupAsset.h"
#include "UFlockManagerComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UFlockManagerComponent
//
//  Replaces: FlockManager.cs (MonoBehaviour, IFlockSettings)
//
//  Drop this onto any AActor to create and drive a flock.
//  On BeginPlay it registers a flock with UFlockSubsystem, pushes all
//  settings/behaviours/states, then spawns ABoidActors.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup = "EvoSwarm", meta = (BlueprintSpawnableComponent))
class EVOSWARMFLOCK_API UFlockManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFlockManagerComponent();

    // ── Identity ──────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flock")
    FName FlockId = TEXT("Flock");

    // ── Spawning ──────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning")
    TSubclassOf<ABoidActor> BoidActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (ClampMin = "0"))
    int32 SpawnCount = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawning", meta = (ClampMin = "0.1"))
    float SpawnRadius = 10.f;

    // ── Configuration assets ──────────────────────────────────────────────────
    /** Boid tuning (movement, perception, avoidance). Replaces BoidProfile. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UBoidSettingsAsset> BoidSettings;

    /** Default behaviour stack applied when no state machine is active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UBehaviourStackAsset> DefaultBehaviourStack;

    /** Optional state machine setup. Leave null for simple flocking. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    TObjectPtr<UStateMachineSetupAsset> StateMachineSetup;

    // ── Anchor (for StayInRadius) ─────────────────────────────────────────────
    /** If true, the flock anchor tracks this component's owner actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bAnchorFollowsOwner = true;

    // ── Timing ────────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    bool bUseFixedTimestep = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.001", EditCondition = "bUseFixedTimestep"))
    float FixedTimestep = 0.016f;

    // ── Runtime data access ───────────────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category = "EvoSwarm")
    int32 GetFlockIndex() const { return FlockIndex; }

    UFUNCTION(BlueprintPure, Category = "EvoSwarm")
    const TArray<ABoidActor*>& GetSpawnedBoids() const { return SpawnedBoids; }

    // ── Dynamic registration (mirrors FlockManager.RegisterBoid) ─────────────
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    int32 RegisterBoid(ABoidActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void  UnregisterBoid(ABoidActor* Actor);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

private:
    int32               FlockIndex = INDEX_NONE;
    TArray<ABoidActor*> SpawnedBoids;

    void SpawnBoids();
    void PushSettingsToFlock(FFlock& Flock);
};
