#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Actor/ABoidActor.h"
#include "UBoidSelectionSubsystem.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoidSelectionChanged,
    const TArray<ABoidActor*>&, SelectedBoids);

// ─────────────────────────────────────────────────────────────────────────────
//  UBoidSelectionSubsystem
//
//  Replaces: SelectionManager.cs (MonoBehaviour — RTS click/box-select)
//
//  ULocalPlayerSubsystem: one instance per local player, no scene actor needed.
//  Handles selection state and issues move orders to UBoidCommandComponents.
//
//  Usage (Blueprint):
//    GetLocalPlayer → GetSubsystem<UBoidSelectionSubsystem>
//    Call SelectBoid / DeselectAll / GiveMoveOrderToSelected
//    Bind OnSelectionChanged to update UI
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class EVOSWARMFLOCK_API UBoidSelectionSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    // ── Selection ─────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm|Selection")
    void SelectBoid(ABoidActor* Actor, bool bAdditive = false);

    UFUNCTION(BlueprintCallable, Category = "EvoSwarm|Selection")
    void DeselectBoid(ABoidActor* Actor);

    UFUNCTION(BlueprintCallable, Category = "EvoSwarm|Selection")
    void DeselectAll();

    /**
     * Box-select: gather all ABoidActors whose screen-space positions fall
     * inside [MinScreen, MaxScreen] (pixel coordinates).
     * Pass bAdditive=true to extend an existing selection (Shift held).
     */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm|Selection")
    void SelectInScreenRect(FVector2D MinScreen, FVector2D MaxScreen, bool bAdditive = false);

    // ── Commands ──────────────────────────────────────────────────────────────
    /** Issue a move order to every currently selected boid. */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm|Selection")
    void GiveMoveOrderToSelected(FVector WorldDestination);

    /** Clear the move order on all selected boids. */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm|Selection")
    void StopSelected();

    // ── Query ─────────────────────────────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category = "EvoSwarm|Selection")
    const TArray<ABoidActor*>& GetSelectedBoids() const { return SelectedBoids; }

    UFUNCTION(BlueprintPure, Category = "EvoSwarm|Selection")
    bool IsSelected(ABoidActor* Actor) const { return SelectedBoids.Contains(Actor); }

    // ── Events ────────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable, Category = "EvoSwarm|Selection")
    FOnBoidSelectionChanged OnSelectionChanged;

private:
    TArray<ABoidActor*> SelectedBoids;

    void ApplySelectionState(ABoidActor* Actor, bool bSelected);
    void BroadcastSelectionChanged();
};
