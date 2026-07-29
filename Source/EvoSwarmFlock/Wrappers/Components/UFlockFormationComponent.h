#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Formations/FormationTypes.h"
#include "Flock/FlockSubsystem.h"
#include "UFlockFormationComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UFlockFormationComponent
//
//  Replaces: UnityFormationController.cs (MonoBehaviour, IFormationController)
//            FormationAsset.cs (ScriptableObject — IFormation type selection)
//
//  Drop on the same AActor as UFlockManagerComponent (or any actor that has
//  access to the FlockIndex). Configure Type and Spacing, set the leader boid
//  index, then call Activate/Deactivate from Blueprint or C++.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup = "EvoSwarm", meta = (BlueprintSpawnableComponent))
class EVOSWARMFLOCK_API UFlockFormationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFlockFormationComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation")
    EFormationType FormationType = EFormationType::Line;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation", meta = (ClampMin = "0.1"))
    float Spacing = 2.f;

    /** Index of the leader boid in FFlock::Boids. Must be set before Activate(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation")
    int32 LeaderBoidIndex = 0;

    /** The flock this controller operates on. Must match the FlockManager on the same actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Formation")
    int32 FlockIndex = INDEX_NONE;

    // UE 5.7: overrides of a UFUNCTION (UActorComponent::Activate/Deactivate are
    // already BlueprintCallable) must not redeclare UFUNCTION().
    void Activate(bool bReset = false) override;

    void Deactivate() override;

    /** Change the formation type at runtime (marks slots dirty so they rebuild). */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void SetFormationType(EFormationType NewType);

    /** Assign a new leader boid index and trigger slot rebuild. */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void SetLeader(int32 NewLeaderIndex);

protected:
    virtual void BeginPlay() override;

private:
    FFlock* GetFlock() const;
};
