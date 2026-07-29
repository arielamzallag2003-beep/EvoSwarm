#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Flock/FlockSubsystem.h"
#include "UBoidCommandComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UBoidCommandComponent
//
//  Replaces: BoidCommandAgent.cs (MonoBehaviour — RTS move orders)
//
//  Attach to an ABoidActor. Supports:
//    GiveMoveOrder()  — set a world-space destination (high-priority Arrival)
//    Stop()           — clear the command target
//    IsSelected       — selection state for RTS UI
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(ClassGroup = "EvoSwarm", meta = (BlueprintSpawnableComponent))
class EVOSWARMFLOCK_API UBoidCommandComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBoidCommandComponent();

    /** Visual scale of the selection highlight ring (procedural disc mesh). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
    float HighlightRadius = 60.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
    FLinearColor HighlightColor = FLinearColor(0.f, 1.f, 0.2f, 0.6f);

    // ── Orders ────────────────────────────────────────────────────────────────
    /** Issue a move order to the boid's simulation data. */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void GiveMoveOrder(FVector Destination);

    /** Clear the current move order. */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void Stop();

    // ── Selection state ───────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void SetSelected(bool bSelected);

    UFUNCTION(BlueprintPure, Category = "EvoSwarm")
    bool IsSelected() const { return bIsSelected; }

    // ── Binding (set automatically by UFlockManagerComponent) ─────────────────
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void BindToBoid(int32 InFlockIndex, int32 InBoidIndex);

protected:
    virtual void BeginPlay() override;

private:
    int32 FlockIndex = INDEX_NONE;
    int32 BoidIndex  = INDEX_NONE;
    bool  bIsSelected = false;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> HighlightMesh;

    void CreateHighlightMesh();
    FBoidData* GetBoidData() const;
};
