#include "Wrappers/Selection/UBoidSelectionSubsystem.h"
#include "Wrappers/Components/UBoidCommandComponent.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Selection management
// ─────────────────────────────────────────────────────────────────────────────
void UBoidSelectionSubsystem::SelectBoid(ABoidActor* Actor, bool bAdditive)
{
    if (!Actor) return;

    if (!bAdditive)
        DeselectAll();

    if (!SelectedBoids.Contains(Actor))
    {
        SelectedBoids.Add(Actor);
        ApplySelectionState(Actor, true);
    }

    BroadcastSelectionChanged();
}

void UBoidSelectionSubsystem::DeselectBoid(ABoidActor* Actor)
{
    if (SelectedBoids.Remove(Actor) > 0)
    {
        ApplySelectionState(Actor, false);
        BroadcastSelectionChanged();
    }
}

void UBoidSelectionSubsystem::DeselectAll()
{
    for (ABoidActor* Actor : SelectedBoids)
        ApplySelectionState(Actor, false);
    SelectedBoids.Reset();
    BroadcastSelectionChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Box selection
//  Mirrors SelectionManager.cs PerformSelection() box branch
// ─────────────────────────────────────────────────────────────────────────────
void UBoidSelectionSubsystem::SelectInScreenRect(FVector2D MinScreen, FVector2D MaxScreen, bool bAdditive)
{
    UWorld* World = GetLocalPlayer()->ViewportClient ? GetLocalPlayer()->ViewportClient->GetWorld() : nullptr;
    if (!World) return;

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC) return;

    if (!bAdditive)
        DeselectAll();

    // Build normalised rect (support dragging in any direction)
    FVector2D RectMin(FMath::Min(MinScreen.X, MaxScreen.X), FMath::Min(MinScreen.Y, MaxScreen.Y));
    FVector2D RectMax(FMath::Max(MinScreen.X, MaxScreen.X), FMath::Max(MinScreen.Y, MaxScreen.Y));

    for (TActorIterator<ABoidActor> It(World); It; ++It)
    {
        ABoidActor* Boid = *It;
        FVector2D ScreenPos;
        if (PC->ProjectWorldLocationToScreen(Boid->GetActorLocation(), ScreenPos, true))
        {
            if (ScreenPos.X >= RectMin.X && ScreenPos.X <= RectMax.X &&
                ScreenPos.Y >= RectMin.Y && ScreenPos.Y <= RectMax.Y)
            {
                if (!SelectedBoids.Contains(Boid))
                {
                    SelectedBoids.Add(Boid);
                    ApplySelectionState(Boid, true);
                }
            }
        }
    }

    BroadcastSelectionChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Commands
// ─────────────────────────────────────────────────────────────────────────────
void UBoidSelectionSubsystem::GiveMoveOrderToSelected(FVector WorldDestination)
{
    for (ABoidActor* Actor : SelectedBoids)
    {
        if (!Actor) continue;
        if (UBoidCommandComponent* Cmd = Actor->FindComponentByClass<UBoidCommandComponent>())
            Cmd->GiveMoveOrder(WorldDestination);
    }
}

void UBoidSelectionSubsystem::StopSelected()
{
    for (ABoidActor* Actor : SelectedBoids)
    {
        if (!Actor) continue;
        if (UBoidCommandComponent* Cmd = Actor->FindComponentByClass<UBoidCommandComponent>())
            Cmd->Stop();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
void UBoidSelectionSubsystem::ApplySelectionState(ABoidActor* Actor, bool bSelected)
{
    if (!Actor) return;
    if (UBoidCommandComponent* Cmd = Actor->FindComponentByClass<UBoidCommandComponent>())
        Cmd->SetSelected(bSelected);
}

void UBoidSelectionSubsystem::BroadcastSelectionChanged()
{
    OnSelectionChanged.Broadcast(SelectedBoids);
}
