#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Flock/FlockContainer.h"
#include "Flock/FlockSubsystem.h"
#include "UFlockObstacleComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UFlockObstacleComponent
//
//  Replaces: FlockObstacle.cs (MonoBehaviour, IObstacle)
//
//  Drop on any AActor that should be treated as an obstacle by boids.
//  Each tick it writes an FObstacleData into every registered flock's
//  Obstacles array. The radius scales with the actor's largest world scale axis
//  to match the Unity behaviour (Radius * max(localScale)).
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup = "EvoSwarm", meta = (BlueprintSpawnableComponent))
class EVOSWARMFLOCK_API UFlockObstacleComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFlockObstacleComponent();

    /** Base radius before scale. Matches FlockObstacle._radius. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle", meta = (ClampMin = "0.01"))
    float BaseRadius = 100.f;

    /** If false, boids ignore this obstacle even though the component exists. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle")
    bool bObstacleActive = true;

    /**
     * Which flocks to register with.
     * Leave empty to register with ALL flocks managed by UFlockSubsystem.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Obstacle")
    TArray<int32> TargetFlockIndices;

    /** Slot index inside the target flock's Obstacles array. -1 = not yet registered. */
    UFUNCTION(BlueprintPure, Category = "EvoSwarm")
    float GetScaledRadius() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* Func) override;

private:
    // Maps FlockIndex → slot index in Flock.Obstacles, so we can update
    // the same slot instead of pushing a new entry every tick.
    TMap<int32, int32> ObstacleSlots;

    void RegisterWithFlocks();
    void UpdateFlockObstacles();
    void UnregisterFromFlocks();
};
