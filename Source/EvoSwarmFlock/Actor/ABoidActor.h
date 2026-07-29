#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABoidActor.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  ABoidActor — thin visual wrapper
//
//  ALL simulation logic lives in UFlockSubsystem.
//  This actor does ONE thing: reads its boid's position from the subsystem
//  and moves the root mesh to match.
//  There are NO behaviour or force calculations here.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType, Blueprintable)
class EVOSWARMFLOCK_API ABoidActor : public AActor
{
    GENERATED_BODY()

public:
    ABoidActor();

    // ── Binding ───────────────────────────────────────────────────────────────
    /** Set which flock and boid index this actor represents. */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void BindToBoid(int32 InFlockIndex, int32 InBoidIndex);

    /** Read back the flock/boid indices (needed by wrapper components). */
    UFUNCTION(BlueprintPure, Category = "EvoSwarm")
    int32 GetFlockIndex() const { return FlockIndex; }

    UFUNCTION(BlueprintPure, Category = "EvoSwarm")
    int32 GetBoidIndex() const { return BoidIndex; }

    // ── AActor overrides ──────────────────────────────────────────────────────
    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> MeshComp;

    UPROPERTY(BlueprintReadOnly, Category = "EvoSwarm")
    int32 FlockIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "EvoSwarm")
    int32 BoidIndex = INDEX_NONE;

private:
    TWeakObjectPtr<class UFlockSubsystem> CachedSubsystem;
};
