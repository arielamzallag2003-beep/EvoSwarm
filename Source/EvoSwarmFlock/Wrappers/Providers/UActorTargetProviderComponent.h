#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Flock/FlockContainer.h"
#include "Flock/FlockSubsystem.h"
#include "UActorTargetProviderComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UActorTargetProviderComponent
//
//  Replaces: TransformTargetProvider.cs (MonoBehaviour, ITargetProvider)
//
//  Attach to an AActor that owns a UFlockManagerComponent.
//  Each tick it pushes:
//    - SeekTargetActor's world position → FBoidData::SeekTarget for all boids
//    - ThreatActors positions          → FFlock::Threats
//    - Physics overlap results          → FFlock::Obstacles (obstacles
//      found dynamically via sphere overlap, matching Unity's OverlapSphere)
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup = "EvoSwarm", meta = (BlueprintSpawnableComponent))
class EVOSWARMFLOCK_API UActorTargetProviderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UActorTargetProviderComponent();

    // ── Seek target ───────────────────────────────────────────────────────────
    /** Actor whose position becomes the seek/arrival target for all boids. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targets")
    TObjectPtr<AActor> SeekTargetActor;

    // ── Threats ───────────────────────────────────────────────────────────────
    /** Actors whose positions are pushed into FFlock::Threats each tick. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threats")
    TArray<TObjectPtr<AActor>> ThreatActors;

    // ── Physics obstacle query ────────────────────────────────────────────────
    /** Collision channel for the per-tick obstacle sphere overlap query. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacles")
    TEnumAsByte<ECollisionChannel> ObstacleQueryChannel = ECC_WorldStatic;

    /**
     * If true, run a sphere overlap each tick to find dynamic obstacles and
     * push them into FFlock::Obstacles. The query radius is the flock's
     * maximum ObstacleAvoidanceDistance across all settings templates.
     * Mirrors Unity's Physics.OverlapSphere in TransformTargetProvider.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacles")
    bool bRunObstacleOverlapQuery = false;

    /** Flock this provider feeds into. Auto-detected from sibling UFlockManagerComponent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targets")
    int32 FlockIndex = INDEX_NONE;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* Func) override;

private:
    void UpdateSeekTargets(FFlock& Flock);
    void UpdateThreats(FFlock& Flock);
    void UpdateObstacles(FFlock& Flock);
};
