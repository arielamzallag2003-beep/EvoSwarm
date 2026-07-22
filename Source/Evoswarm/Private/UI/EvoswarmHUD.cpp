// Copyright Evoswarm.

#include "EvoswarmHUD.h"
#include "SEvoswarmHUD.h"
#include "EvoswarmSimSubsystem.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

void AEvoswarmHUD::BeginPlay()
{
	Super::BeginPlay();

	// No HUD on a dedicated server, and nothing to show without a viewport.
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UGameViewportClient* Viewport = World->GetGameViewport();
	if (!Viewport)
	{
		return;
	}

	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim = World->GetSubsystem<UEvoswarmSimSubsystem>();

	// The widget tolerates a not-yet-ready sim: it builds the species rows lazily on the first
	// tick that reports species, so HUD/GameMode BeginPlay ordering doesn't matter.
	SAssignNew(HudWidget, SEvoswarmHUD).Sim(Sim);

	Viewport->AddViewportWidgetContent(HudWidget.ToSharedRef(), /*ZOrder*/ 10);
}

void AEvoswarmHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HudWidget.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameViewportClient* Viewport = World->GetGameViewport())
			{
				Viewport->RemoveViewportWidgetContent(HudWidget.ToSharedRef());
			}
		}
		HudWidget.Reset();
	}

	Super::EndPlay(EndPlayReason);
}