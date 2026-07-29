#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "Flock/FlockContainer.h"
#include "FlockSubsystem.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UFlockSubsystem
//
//  Replaces: class Flock : IFlock  (the simulation-driver side)
//  All flocks and their boids are owned here.
//  This is the only place where tick, spatial, force, and integration
//  logic runs — NOT in any AActor.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class EVOSWARMFLOCK_API UFlockSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ── Lifecycle ─────────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

    // ── Tick ─────────────────────────────────────────────────────────────────
    // Called by the subsystem's tick registration (see .cpp)
    void Tick(float DeltaTime);

    // ── Flock management ──────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    int32 CreateFlock(FName FlockId);

    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    bool  DestroyFlock(int32 FlockIndex);

    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    int32 AddBoidToFlock(int32 FlockIndex, const FBoidData& InitialData);

    /** Add a boid with a specific genome (used by SpawnChild during reproduction). */
    int32 AddBoidToFlockWithGenome(
        int32 FlockIndex,
        const FBoidData& InitialData,
        const FFlockGenome& Genome);

    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void  DeactivateBoid(int32 FlockIndex, int32 BoidIndex);

    // ── Data access ───────────────────────────────────────────────────────────
    FFlock*       GetFlock(int32 FlockIndex);
    const FFlock* GetFlock(int32 FlockIndex) const;

    /** Read-only boid position (used by ABoidActor to move its mesh). */
    bool GetBoidTransform(int32 FlockIndex, int32 BoidIndex,
                          FVector& OutPosition, FVector& OutForward) const;

    // ── Public flock data (indexed by FlockIndex) ────────────────────────────
    TArray<FFlock> AllFlocks;

private:
    // ── Tick internals ────────────────────────────────────────────────────────
    void TickFlock(FFlock& Flock, float DeltaTime);

    /** Pass 1: rebuild spatial grid + neighbor lists. */
    void Pass_RebuildSpatial(FFlock& Flock);

    /** Pass 2: compute forces for every active boid. */
    void Pass_ComputeForces(FFlock& Flock, float DeltaTime);

    /** Pass 3: integrate forces → update position/velocity. */
    void Pass_Integrate(FFlock& Flock, float DeltaTime);

    /**
     * Pass 4: update stats (stamina, HP regen, age, reproduction).
     * Runs after Pass_Integrate. Spawns child boids as needed.
     */
    void Pass_UpdateStats(FFlock& Flock, float DeltaTime);

    /** Spawn a mutated child boid near the parent. */
    void SpawnChild(FFlock& Flock, int32 ParentIndex);

    /** Apply plane constraint so boids stay on the configured movement plane. */
    static void EnforcePlaneConstraint(FBoidData& Boid, EMovementPlane Plane);

    /** FTSTicker callback — delegates to Tick(). */
    bool TickFromDelegate(float DeltaTime);

    FTSTicker::FDelegateHandle TickHandle;
};
