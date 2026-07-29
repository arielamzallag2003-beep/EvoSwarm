// Copyright Evoswarm.

#include "EvoswarmHUD.h"
#include "SEvoswarmHUD.h"
#include "EvoswarmSimSubsystem.h"
#include "EvoswarmTuning.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "HAL/FileManager.h"

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

void AEvoswarmHUD::TogglePanel()
{
	bShowPanel = !bShowPanel;
	if (HudWidget.IsValid())
	{
		HudWidget->SetVisibility(bShowPanel ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
}

void AEvoswarmHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	UFont* Small = GEngine ? GEngine->GetSmallFont() : nullptr;

	// Build stamp (top-right): the modified time of the loaded module binary. If this doesn't
	// match your last compile, the editor is running a stale DLL / the wrong project. Cached
	// once — file stat is not a per-frame operation.
	if (BuildStamp.IsEmpty())
	{
		// The editor can stat the loaded module DLL (catches a stale/wrong binary). A packaged
		// Shipping build has no module-filename API, so fall back to the executable's timestamp.
#if WITH_EDITOR
		const FString ModulePath = FModuleManager::Get().GetModuleFilename(TEXT("Evoswarm"));
#else
		const FString ModulePath = FPlatformProcess::ExecutablePath();
#endif
		// GetTimeStamp is UTC; shift to local time so it matches the clock it was built at.
		const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*ModulePath) + (FDateTime::Now() - FDateTime::UtcNow());
		BuildStamp = FString::Printf(TEXT("build %s"), *Stamp.ToString(TEXT("%Y-%m-%d %H:%M")));
	}
	DrawText(BuildStamp, FLinearColor(0.75f, 0.78f, 0.82f, 0.85f), Canvas->SizeX - 168.f, 10.f, Small);

	// Day/night clock just below the build stamp, tinted by daylight (info the Slate panel omits).
	const int32 ClockH = FMath::FloorToInt(Evo::GSkyHour);
	const int32 ClockM = FMath::FloorToInt((Evo::GSkyHour - ClockH) * 60.f);
	const bool bDay = Evo::GDaylight > 0.5f;
	const FLinearColor ClockCol = bDay ? FLinearColor(1.0f, 0.85f, 0.4f, 1.f) : FLinearColor(0.6f, 0.68f, 0.95f, 1.f);
	DrawText(FString::Printf(TEXT("%s %02d:%02d"), bDay ? TEXT("Day") : TEXT("Night"), ClockH, ClockM),
		ClockCol, Canvas->SizeX - 168.f, 26.f, Small);

	// --- Accélérateur de temps : facteur courant + finesse du pas d'intégration ---
	// Le pas est affiché parce qu'il est la vraie mesure de crédibilité de la nuée à vitesse
	// élevée : à facteur égal, un pas de 30 ms donne un troupeau cohérent là où 300 ms donne
	// des sauts. Il vaut donc mieux le voir que le deviner.
	UWorld* World = GetWorld();
	const UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	if (Sim)
	{
		const bool bPaused = Sim->IsPaused();
		const float Speed = Sim->GetRequestedSpeed();
		const float Effective = Sim->GetEffectiveSpeed();

		// Quand le rendu ne suit pas, le pas est écrêté et la simulation avance moins vite que
		// demandé : on affiche « x50 -> x31 » plutôt que de laisser croire au facteur affiché.
		FString SpeedPart = Evo::SpeedLabel(Speed);
		if (!bPaused && Effective < Speed * 0.9f)
		{
			SpeedPart += FString::Printf(TEXT(" -> x%.0f"), Effective);
		}
		const FString Label = bPaused
			? FString(TEXT("PAUSE"))
			: FString::Printf(TEXT("%s%s  %.0f ms/frame"), *SpeedPart,
				Sim->IsTurboActive() ? TEXT(" turbo") : TEXT(""), Sim->GetSimStepSeconds() * 1000.f);

		// Gris tant qu'on est en temps réel, ambre dès qu'on accélère, bleu en pause : le
		// facteur ne doit jamais pouvoir être oublié pendant une démo.
		const FLinearColor SpeedCol = bPaused
			? FLinearColor(0.55f, 0.72f, 1.0f, 1.f)
			: (Speed > 1.f ? FLinearColor(1.0f, 0.72f, 0.30f, 1.f)
				: (Speed < 1.f ? FLinearColor(0.65f, 0.85f, 0.95f, 1.f)
					: FLinearColor(0.75f, 0.78f, 0.82f, 0.85f)));
		DrawText(Label, SpeedCol, Canvas->SizeX - 168.f, 42.f, Small);
	}
}
