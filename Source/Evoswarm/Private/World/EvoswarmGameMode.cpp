// Copyright Evoswarm.

#include "EvoswarmGameMode.h"
#include "SpeciesConfig.h"
#include "BoidStats.h"
#include "BoidFragments.h"
#include "EvoswarmTuning.h"
#include "EvoswarmSimSubsystem.h"
#include "EvoswarmSpeciesRenderer.h"
#include "EvoswarmTerrainActor.h"
#include "EvoswarmTerrain.h"
#include "EvoswarmHUD.h"
#include "EvoswarmPawn.h"

#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator
#include "TimerManager.h"
#include "UnrealClient.h" // FScreenshotRequest (dev -EvoScreenshot utility)
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"     // ASkyAtmosphere
#include "Components/VolumetricCloudComponent.h"   // AVolumetricCloud
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "Curves/CurveLinearColor.h"

DEFINE_LOG_CATEGORY_STATIC(LogEvoswarm, Log, All);

AEvoswarmGameMode::AEvoswarmGameMode()
{
	// Free-flying spectator pawn (ZQSD) + debug-toggle key.
	DefaultPawnClass = AEvoswarmPawn::StaticClass();
	HUDClass = AEvoswarmHUD::StaticClass();
}

void AEvoswarmGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	if (!Sim)
	{
		UE_LOG(LogEvoswarm, Error, TEXT("Evoswarm sim subsystem unavailable; cannot start simulation."));
		return;
	}

	// Dev override: `-EvoStartHour=17.5` starts the day/night cycle at a given hour (handy to
	// verify dusk/night lighting headlessly without editing the game mode defaults).
	FParse::Value(FCommandLine::Get(), TEXT("EvoStartHour="), GoodSkyStartHour);

	// Wipe pre-existing scene clutter first so we build onto a clean slate.
	if (bCleanSceneOnPlay)
	{
		CleanScene();
	}

	// Generate default species if none were authored.
	if (Species.Num() == 0)
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Species.Add(MakeDefaultSpecies(Index));
		}
	}

	if (bBuildEnvironment)
	{
		BuildEnvironment();
	}

	SpawnSpecies(*Sim);

	Sim->StartSimulation();

	UE_LOG(LogEvoswarm, Log, TEXT("Evoswarm started with %d species."), Species.Num());

	// Dev utility: `-EvoScreenshot` (e.g. with -game -RenderOffscreen) frames the world and
	// saves a screenshot — lets tooling eyeball the build headless, and captures presentation
	// stills. Optional knobs: -EvoShotDelay=<s>, -EvoShotZ=<cm>, -EvoShotPitch=<deg>,
	// -EvoShotYaw=<deg>, -EvoShotName=<file>, -EvoShotNoUI, -EvoShotAnalytics[=<page>],
	// -EvoShotDebug=<0..4>.
	if (FParse::Param(FCommandLine::Get(), TEXT("EvoScreenshot")))
	{
		float Delay = 25.f;   FParse::Value(FCommandLine::Get(), TEXT("EvoShotDelay="), Delay);
		float ShotZ = 12000.f; FParse::Value(FCommandLine::Get(), TEXT("EvoShotZ="), ShotZ);
		float Pitch = -14.f;  FParse::Value(FCommandLine::Get(), TEXT("EvoShotPitch="), Pitch);
		float Yaw = 35.f;     FParse::Value(FCommandLine::Get(), TEXT("EvoShotYaw="), Yaw);
		FString ShotName = TEXT("EvoVerify"); FParse::Value(FCommandLine::Get(), TEXT("EvoShotName="), ShotName);
		const bool bShotUI = !FParse::Param(FCommandLine::Get(), TEXT("EvoShotNoUI"));
		int32 DebugMode = -1; FParse::Value(FCommandLine::Get(), TEXT("EvoShotDebug="), DebugMode);
		int32 AnalyticsPage = -1;
		const bool bWantAnalytics = FParse::Param(FCommandLine::Get(), TEXT("EvoShotAnalytics"))
			|| FParse::Value(FCommandLine::Get(), TEXT("EvoShotAnalytics="), AnalyticsPage);

		FTimerHandle ShotTimer;
		GetWorldTimerManager().SetTimer(ShotTimer, FTimerDelegate::CreateWeakLambda(this,
			[this, ShotZ, Pitch, Yaw, ShotName, bShotUI, DebugMode, bWantAnalytics, AnalyticsPage]()
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				if (APawn* P = PC->GetPawn())
				{
					P->SetActorLocation(FVector(0.f, 0.f, ShotZ));
					PC->SetControlRotation(FRotator(Pitch, Yaw, 0.f));
				}
			}
			if (UEvoswarmSimSubsystem* S = GetWorld()->GetSubsystem<UEvoswarmSimSubsystem>())
			{
				if (DebugMode >= 0)
				{
					S->ToggleDebugDraw();
					S->SetDebugMode(DebugMode);
				}
				if (bWantAnalytics)
				{
					S->ToggleAnalytics();
					for (int32 I = 0; I < FMath::Max(0, AnalyticsPage); ++I)
					{
						S->CycleAnalyticsPage(1);
					}
				}
			}
			FTimerHandle InnerTimer;
			GetWorldTimerManager().SetTimer(InnerTimer, [ShotName, bShotUI]()
			{
				FScreenshotRequest::RequestScreenshot(ShotName, bShotUI, /*bAddFilenameSuffix=*/true);
			}, 2.5f, false);
		}), Delay, false);
	}
}

void AEvoswarmGameMode::CleanScene()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Remove anything that would conflict with our procedural environment: existing
	// lighting/sky/fog/post-process, static-mesh props/floors, landscapes, and any leftover
	// Evoswarm actors from a saved level. Boids/food are Mass entities (not actors) so untouched.
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A)
		{
			continue;
		}
		const bool bConflicts =
			A->IsA(ADirectionalLight::StaticClass()) ||
			A->IsA(ASkyLight::StaticClass()) ||
			A->IsA(ASkyAtmosphere::StaticClass()) ||
			A->IsA(AExponentialHeightFog::StaticClass()) ||
			A->IsA(AVolumetricCloud::StaticClass()) ||
			A->IsA(APostProcessVolume::StaticClass()) ||
			A->IsA(AStaticMeshActor::StaticClass()) ||
			A->IsA(AEvoswarmTerrain::StaticClass()) ||
			A->IsA(AEvoswarmSpeciesRenderer::StaticClass()) ||
			A->GetClass()->GetName().Contains(TEXT("Landscape"));
		if (bConflicts)
		{
			ToDestroy.Add(A);
		}
	}

	for (AActor* A : ToDestroy)
	{
		A->Destroy();
	}
}

void AEvoswarmGameMode::BuildEnvironment()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;

	// Bake the terrain height/biome cache first: everything after this (terrain mesh, prop
	// scatter, food seeding, and every per-frame boid query) samples the cache instead of
	// re-evaluating ~13 Perlin octaves per lookup. Slightly padded so look-ahead probes
	// beyond the arena edge stay on the fast path.
	Evo::InitTerrainCache(Evo::ArenaHalfExtent * 1.03f, 1280);

	// The procedural biome terrain IS the ground (replaces the old flat plane). Always built.
	// Resolution follows the arena size so quads stay ~2 m whatever the map extent.
	if (AEvoswarmTerrain* Terrain = World->SpawnActor<AEvoswarmTerrain>(AEvoswarmTerrain::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		const int32 CellsPerSide = FMath::Clamp(FMath::RoundToInt(Evo::ArenaHalfExtent / 110.f), 64, 512);
		Terrain->Build(Evo::ArenaHalfExtent, CellsPerSide);
	}

	// Helper: is there already an actor of this class in the level?
	auto HasActor = [World](UClass* Cls) -> bool
	{
		for (TActorIterator<AActor> It(World, Cls); It; ++It) { return true; }
		return false;
	};

	// --- Sun: high, bright, drives the sky atmosphere (stylized midday) ---
	// Kept as a named pointer: when GoodSky is active it takes ownership of this light and
	// animates its rotation/intensity/colour along the day/night cycle.
	ADirectionalLight* Sun = nullptr;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It) { Sun = *It; break; }
	if (!Sun)
	{
		Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector::ZeroVector, FRotator(-52.f, -40.f, 0.f), Params);
		if (Sun)
		{
			Sun->SetMobility(EComponentMobility::Movable);
			if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
			{
				C->SetIntensity(10.f);
				C->SetLightColor(FLinearColor(1.0f, 0.96f, 0.86f)); // warm sunlight
				C->SetAtmosphereSunLight(true); // this light defines the sky's sun
						C->SetDynamicShadowCascades(3); // 3 cascades is a good quality/cost balance for this view
				// Only shadow the near scene (~300 m). Creatures you actually look at stay grounded;
				// distant shadows (invisible on a 1 km map) don't render — a big shadow-pass saving.
				C->DynamicShadowDistanceMovableLight = 30000.f;
			}
		}
	}

	// --- GoodSky day/night dome (replaces the engine sky when available) ---
	AActor* GoodSky = nullptr;
	if (bUseGoodSky)
	{
		GoodSky = SpawnGoodSky(Sun);
	}
	GoodSkyActor = GoodSky;
	SkyTimeOfDay = GoodSkyStartHour;
	SkyClockStartSeconds = World->GetTimeSeconds();

	// --- Procedural sky atmosphere (gradient sky + horizon glow, no textures) ---
	// Skipped when GoodSky is active: the dome IS the sky, and a second sky (or the engine's
	// volumetric clouds) would fight it visually.
	if (!GoodSky && !HasActor(ASkyAtmosphere::StaticClass()))
	{
		World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	}

	// --- Sky light capturing the live atmosphere + clouds for vibrant ambient bounce ---
	if (!HasActor(ASkyLight::StaticClass()))
	{
		if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0.f, 0.f, 1500.f), FRotator::ZeroRotator, Params))
		{
			if (USkyLightComponent* C = Sky->GetLightComponent())
			{
				C->SetMobility(EComponentMobility::Movable);
				C->SetRealTimeCapture(true);
				C->bLowerHemisphereIsBlack = false;
				C->SetIntensity(1.25f); // richer sky ambient fill
			}
		}
	}

	// --- Light atmospheric haze with volumetric fog (depth + soft god-rays) ---
	if (!HasActor(AExponentialHeightFog::StaticClass()))
	{
		if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator, Params))
		{
			if (UExponentialHeightFogComponent* C = Fog->GetComponent())
			{
				C->SetFogDensity(0.012f);
				C->SetFogInscatteringColor(FLinearColor(0.6f, 0.75f, 0.95f));
				C->bEnableVolumetricFog = true;
			}
		}
	}

	// --- Procedural volumetric clouds (engine default cloud material) ---
	if (!GoodSky && !HasActor(AVolumetricCloud::StaticClass()))
	{
		if (AVolumetricCloud* Cloud = World->SpawnActor<AVolumetricCloud>(AVolumetricCloud::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
		{
			if (UVolumetricCloudComponent* C = Cloud->FindComponentByClass<UVolumetricCloudComponent>())
			{
				if (UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud.m_SimpleVolumetricCloud")))
				{
					C->SetMaterial(CloudMat);
				}
			}
		}
	}

	// --- Unbound post-process: punchy bloom, vibrant saturation, light vignette, high-quality GI ---
	if (!HasActor(APostProcessVolume::StaticClass()))
	{
		if (APostProcessVolume* PP = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
		{
			PP->bUnbound = true;
			PP->Priority = 1.f;
			FPostProcessSettings& S = PP->Settings;

			S.bOverride_BloomIntensity = true;          S.BloomIntensity = 0.8f;
			S.bOverride_BloomThreshold = true;          S.BloomThreshold = -0.3f;   // soft overall glow
			S.bOverride_ColorSaturation = true;          S.ColorSaturation = FVector4(1.2f, 1.2f, 1.2f, 1.0f);
			S.bOverride_ColorContrast = true;            S.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.0f);
			S.bOverride_VignetteIntensity = true;        S.VignetteIntensity = 0.28f;
			S.bOverride_AmbientOcclusionIntensity = true; S.AmbientOcclusionIntensity = 0.55f; // grounds the boids/props
			// Gentle cinematic split-tone: cool shadows, warm highlights.
			S.bOverride_ColorGainShadows = true;         S.ColorGainShadows = FVector4(0.92f, 0.97f, 1.08f, 1.0f);
			S.bOverride_ColorGainHighlights = true;      S.ColorGainHighlights = FVector4(1.08f, 1.02f, 0.9f, 1.0f);
			S.bOverride_LumenFinalGatherQuality = true;  S.LumenFinalGatherQuality = 2.0f;
		}
	}

	// Keep the height fog's colour in step with the sun: GoodSky animates the light, but the
	// fog was a fixed pale blue — at dusk/night the whole horizon band stayed daylight-blue.
	SunLight = Sun;
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It) { FogActor = *It; break; }
	if (SunLight.IsValid() && FogActor.IsValid())
	{
		GetWorldTimerManager().SetTimer(FogSyncTimer, this, &AEvoswarmGameMode::UpdateFogWithSky, 0.25f, true);
		UpdateFogWithSky();
	}
}

// Implemented after the reflection helpers below (it reads GoodSky's own horizon curve).

namespace
{
	// BP_GoodSky is a Blueprint, so its variables are reached by name via reflection — the
	// module keeps zero compile-time dependency on the pack and still runs without it.
	// Blueprint variable names keep their display spelling ("Enable Auto Day / Night Cycle
	// In Game?"), so names are compared stripped of everything but letters and digits.
	FString SanitizePropName(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len());
		for (TCHAR C : In)
		{
			if (FChar::IsAlnum(C))
			{
				Out.AppendChar(FChar::ToLower(C));
			}
		}
		return Out;
	}

	FProperty* FindPropFuzzy(UObject* Obj, const TCHAR* Wanted)
	{
		const FString Target = SanitizePropName(Wanted);
		for (TFieldIterator<FProperty> It(Obj->GetClass()); It; ++It)
		{
			const FString Name = SanitizePropName(It->GetName());
			// EndsWith also catches category-prefixed spellings ("Global_Global Time Speed").
			if (Name == Target || Name.EndsWith(Target))
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool SetBoolProp(UObject* Obj, const TCHAR* Name, bool bValue)
	{
		if (FBoolProperty* P = CastField<FBoolProperty>(FindPropFuzzy(Obj, Name)))
		{
			P->SetPropertyValue_InContainer(Obj, bValue);
			return true;
		}
		return false;
	}

	bool SetFloatProp(UObject* Obj, const TCHAR* Name, double Value)
	{
		// Blueprint floats are doubles in UE5; accept either width.
		if (FNumericProperty* P = CastField<FNumericProperty>(FindPropFuzzy(Obj, Name)))
		{
			if (P->IsFloatingPoint())
			{
				P->SetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(Obj), Value);
				return true;
			}
		}
		return false;
	}

	bool SetObjectProp(UObject* Obj, const TCHAR* Name, UObject* Value)
	{
		if (FObjectPropertyBase* P = CastField<FObjectPropertyBase>(FindPropFuzzy(Obj, Name)))
		{
			if (Value && Value->IsA(P->PropertyClass))
			{
				P->SetObjectPropertyValue_InContainer(Obj, Value);
				return true;
			}
		}
		return false;
	}

	bool GetDoubleProp(UObject* Obj, const TCHAR* Name, double& OutValue)
	{
		if (FNumericProperty* P = CastField<FNumericProperty>(FindPropFuzzy(Obj, Name)))
		{
			if (P->IsFloatingPoint())
			{
				OutValue = P->GetFloatingPointPropertyValue(P->ContainerPtrToValuePtr<void>(Obj));
				return true;
			}
		}
		return false;
	}

	UObject* GetObjectProp(UObject* Obj, const TCHAR* Name)
	{
		if (FObjectPropertyBase* P = CastField<FObjectPropertyBase>(FindPropFuzzy(Obj, Name)))
		{
			return P->GetObjectPropertyValue_InContainer(Obj);
		}
		return nullptr;
	}

	// Sets a Blueprint enum property by the entry's DISPLAY name (user-defined enums store
	// raw names as NewEnumeratorX, so the pretty text is the only stable identifier).
	bool SetEnumPropByEntryName(UObject* Obj, const TCHAR* PropName, const TCHAR* EntryContains)
	{
		FProperty* P = FindPropFuzzy(Obj, PropName);
		UEnum* Enum = nullptr;
		if (FEnumProperty* EP = CastField<FEnumProperty>(P))
		{
			Enum = EP->GetEnum();
		}
		else if (FByteProperty* BP = CastField<FByteProperty>(P))
		{
			Enum = BP->Enum;
		}
		if (!Enum)
		{
			return false;
		}
		const FString Target = SanitizePropName(EntryContains);
		for (int32 I = 0; I < Enum->NumEnums() - 1; ++I)
		{
			if (SanitizePropName(Enum->GetDisplayNameTextByIndex(I).ToString()).Contains(Target))
			{
				const int64 Value = Enum->GetValueByIndex(I);
				if (FEnumProperty* EP = CastField<FEnumProperty>(P))
				{
					EP->GetUnderlyingProperty()->SetIntPropertyValue(EP->ContainerPtrToValuePtr<void>(Obj), Value);
				}
				else if (FByteProperty* BP = CastField<FByteProperty>(P))
				{
					BP->SetPropertyValue_InContainer(Obj, static_cast<uint8>(Value));
				}
				return true;
			}
		}
		return false;
	}
}

void AEvoswarmGameMode::UpdateFogWithSky()
{
	if (!FogActor.IsValid())
	{
		return;
	}

	// C++ owns the day clock and drives it the way the pack itself is built to be driven:
	// rotate the SUN along the day arc, then set the self-clearing "Refresh Sky Shader
	// ( For direction actor )" flag and call Init — GoodSky derives its hour FROM the light
	// and repaints the dome to match. Sun, dome and fog stay consistent by construction.
	FLinearColor Col(0.f, 0.f, 0.f);
	bool bGotColor = false;
	if (GoodSkyActor.IsValid() && SunLight.IsValid())
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - SkyClockStartSeconds;
		const double DayLenSec = FMath::Max(10.f, GoodSkyDayLengthMinutes * 60.f);
		SkyTimeOfDay = FMath::Fmod(GoodSkyStartHour + Elapsed * 24.0 / DayLenSec, 24.0);

		// 6h = sunrise on the horizon, 12h = zenith, 18h = sunset, night below the horizon.
		const float SunPitch = static_cast<float>(-(SkyTimeOfDay - 6.0) * 15.0);
		SunLight->SetActorRotation(FRotator(SunPitch, -40.f, 0.f));

		GoodSkyActor->SetActorTickEnabled(false); // our 4 Hz driver replaces the BP tick
		SetBoolProp(GoodSkyActor.Get(), TEXT("Use All Random"), false);
		SetBoolProp(GoodSkyActor.Get(), TEXT("UseRandomTime( For Custom Mode )"), false);
		SetBoolProp(GoodSkyActor.Get(), TEXT("Refresh Sky Shader( For direction actor )"), true);
		if (GoodSkyUpdateFunc != NAME_None)
		{
			if (UFunction* Fn = GoodSkyActor->FindFunction(GoodSkyUpdateFunc))
			{
				GoodSkyActor->ProcessEvent(Fn, nullptr);
			}
		}

		// GoodSky's own derived hour (written back by Init) — canonical for the fog colour.
		double DomeHour = SkyTimeOfDay;
		GetDoubleProp(GoodSkyActor.Get(), TEXT("Time of Day ( For Custom Mode )"), DomeHour);

		// Publish the day clock for the sim: creatures key their activity off this.
		Evo::GSkyHour = static_cast<float>(DomeHour);
		Evo::GDaylight = Evo::DaylightFromHour(Evo::GSkyHour);
		UE_LOG(LogEvoswarm, Verbose, TEXT("GoodSky live: clock=%.2f domeHour=%.2f sunPitch=%.1f"), SkyTimeOfDay, DomeHour, SunPitch);

		// Fog = the dome's own background-horizon colour at the same hour, so the band of
		// fog below the horizon is exactly the colour the dome fades into, day and night.
		UCurveLinearColor* Curve = Cast<UCurveLinearColor>(GetObjectProp(GoodSkyActor.Get(), TEXT("Curve_BackGroundHorizonColor")));
		if (!Curve)
		{
			Curve = Cast<UCurveLinearColor>(GetObjectProp(GoodSkyActor.Get(), TEXT("Curve_DomeColor")));
		}
		if (Curve)
		{
			Col = Curve->GetLinearColorValue(static_cast<float>(DomeHour));
			// The dome material is emissive; tame HDR values so the fog doesn't glow.
			const float MaxC = FMath::Max3(Col.R, Col.G, Col.B);
			if (MaxC > 1.f)
			{
				Col *= 1.f / MaxC;
			}
			bGotColor = true;
		}
	}

	// Fallback (GoodSky absent): tint from the sun's elevation instead.
	if (!bGotColor && SunLight.IsValid())
	{
		const float SunHeight = FMath::Clamp(-SunLight->GetActorForwardVector().Z, -1.f, 1.f);
		const float Day = FMath::SmoothStep(0.02f, 0.35f, SunHeight);
		const float Dusk = FMath::Clamp(1.f - FMath::Abs(SunHeight) * 3.5f, 0.f, 1.f);
		Col = FMath::Lerp(FLinearColor(0.015f, 0.02f, 0.045f), FLinearColor(0.55f, 0.72f, 0.92f), Day);
		Col = FMath::Lerp(Col, FLinearColor(0.90f, 0.48f, 0.32f), Dusk * 0.8f);
		bGotColor = true;
	}

	if (bGotColor)
	{
		if (UExponentialHeightFogComponent* C = FogActor->GetComponent())
		{
			C->SetFogInscatteringColor(Col);
		}
	}
}

AActor* AEvoswarmGameMode::SpawnGoodSky(ADirectionalLight* Sun)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* SkyClass = LoadClass<AActor>(nullptr, TEXT("/Game/GoodSky/Blueprint/BP_GoodSky.BP_GoodSky_C"));
	if (!SkyClass)
	{
		UE_LOG(LogEvoswarm, Warning, TEXT("GoodSky pack not found (Content/GoodSky); using the engine sky instead."));
		return nullptr;
	}

	// Diagnostic aid: `-LogCmds="LogEvoswarm Verbose"` lists the pack's real variable names,
	// so a pack update that renames variables is easy to re-wire.
	for (TFieldIterator<FProperty> It(SkyClass); It; ++It)
	{
		UE_LOG(LogEvoswarm, Verbose, TEXT("GoodSky property: '%s' (%s)"), *It->GetName(), *It->GetClass()->GetName());
	}
	for (TFieldIterator<UFunction> It(SkyClass); It; ++It)
	{
		UE_LOG(LogEvoswarm, Verbose, TEXT("GoodSky function: '%s' (%d parms, %d bytes)"), *It->GetName(), It->NumParms, It->ParmsSize);
	}

	// Reuse a dome the user may have placed in the level rather than stacking a second sky.
	AActor* Sky = nullptr;
	for (TActorIterator<AActor> It(World, SkyClass); It; ++It) { Sky = *It; break; }
	if (!Sky)
	{
		// Deferred spawn: variables are set before FinishSpawning so the construction script
		// (which builds the dome from them) already sees the final values.
		Sky = World->SpawnActorDeferred<AActor>(SkyClass, FTransform::Identity);
		if (!Sky)
		{
			UE_LOG(LogEvoswarm, Warning, TEXT("Failed to spawn BP_GoodSky; using the engine sky instead."));
			return nullptr;
		}

		// Hand GoodSky our sun so its curves drive the light's rotation/intensity/colour.
		// (Names as they appear in BP_GoodSky; the leading "└" tree glyphs sanitize away.)
		// The pack's auto day/night cycle runs its OWN clock and ignores the start hour, so
		// it stays OFF: C++ owns the clock (UpdateFogWithSky pushes the hour every tick) —
		// which also keeps the fog band exactly in step with the dome.
		const bool bLight = Sun && SetObjectProp(Sky, TEXT("Directional light actor ( For Custom Mode )"), Sun);
		const bool bTod   = SetBoolProp(Sky, TEXT("Enable Time of Day"), true);
		const bool bAuto  = SetBoolProp(Sky, TEXT("Enable Auto Day / Night Cycle In Game?"), false);
		const bool bHour  = SetFloatProp(Sky, TEXT("Time of Day ( For Custom Mode )"), GoodSkyStartHour);
		// Without this the dome stays glued to the "Daytime" PRESET and ignores the hour:
		// the preset selector must point at the "Custom Mode ( From Time Of Day )" entry.
		const bool bPreset = SetEnumPropByEntryName(Sky, TEXT("SkyPreset"), TEXT("Custom Mode"));
		// CRITICAL: the pack ships with "Use All Random" randomisation which re-rolls the
		// preset/hour inside Init and silently overwrote everything set above (readback
		// showed tod=11.47 preset=0 enableTod=0). Both random modes must be off.
		const bool bRand  = SetBoolProp(Sky, TEXT("Use All Random"), false);
		const bool bRandT = SetBoolProp(Sky, TEXT("UseRandomTime( For Custom Mode )"), false);
		// ...and so must "Refresh Sky Shader (For direction actor)": with it on, Init derives
		// the hour FROM the light's current rotation (our -52deg sun = 11.47h) and overwrites
		// the custom hour every call. Off, the flow is the right way round: hour drives light.
		const bool bNoDeriv = SetBoolProp(Sky, TEXT("Refresh Sky Shader( For direction actor )"), false);
		UE_LOG(LogEvoswarm, Log,
			TEXT("GoodSky spawned (light=%d timeOfDay=%d autoOff=%d startHour=%d preset=%d randOff=%d/%d noDeriv=%d) — start %.1fh, day %.1f min, C++-driven clock."),
			bLight ? 1 : 0, bTod ? 1 : 0, bAuto ? 1 : 0, bHour ? 1 : 0, bPreset ? 1 : 0, bRand ? 1 : 0, bRandT ? 1 : 0, bNoDeriv ? 1 : 0, GoodSkyStartHour, GoodSkyDayLengthMinutes);

		Sky->FinishSpawning(FTransform::Identity);

		// The BP's Tick ("GoodSky Realtime Update") re-derives the hour from the light's
		// rotation every frame — with a static light that pinned the sky to 11.47h and
		// overwrote everything set above. Our 4 Hz driver replaces it outright.
		Sky->SetActorTickEnabled(false);
	}

	// Find the BP's zero-parameter refresh entry point so pushing a new hour re-applies the
	// sun rotation + dome material. "Init" is the pack's apply-my-variables function (its
	// only real public API — the others are the auto-cycle tick / random presets).
	static const TCHAR* UpdateFnCandidates[] = {
		TEXT("Init"),
		TEXT("GoodSky Realtime Update") };
	for (const TCHAR* FnName : UpdateFnCandidates)
	{
		if (UFunction* Fn = Sky->FindFunction(FName(FnName)))
		{
			if (Fn->NumParms == 0)
			{
				GoodSkyUpdateFunc = FName(FnName);
				break;
			}
		}
	}
	UE_LOG(LogEvoswarm, Log, TEXT("GoodSky refresh function: %s"), GoodSkyUpdateFunc == NAME_None ? TEXT("<none found>") : *GoodSkyUpdateFunc.ToString());

	return Sky;
}

void AEvoswarmGameMode::SpawnSpecies(UEvoswarmSimSubsystem& Sim)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Food renderer: a low-poly grass tuft instanced for every plant.
	UStaticMesh* GrassMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/EvoGen/Evo_Grass.Evo_Grass"));
	if (AEvoswarmSpeciesRenderer* FoodRenderer = World->SpawnActor<AEvoswarmSpeciesRenderer>(AEvoswarmSpeciesRenderer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator))
	{
		FoodRenderer->SetupAppearance(GrassMesh, FLinearColor(0.22f, 0.62f, 0.18f));
		FoodRenderer->SetCullDistance(24000.f, 36000.f); // grass tufts are tiny — cull soonest
		Sim.RegisterFoodISM(FoodRenderer->GetISM());
	}

	// Carcass renderer: a low-poly lump instanced for every kill.
	UStaticMesh* CarcassMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/EvoGen/Evo_Carcass.Evo_Carcass"));
	if (AEvoswarmSpeciesRenderer* CarcassRenderer = World->SpawnActor<AEvoswarmSpeciesRenderer>(AEvoswarmSpeciesRenderer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator))
	{
		CarcassRenderer->SetupAppearance(CarcassMesh, FLinearColor(0.35f, 0.05f, 0.05f));
		CarcassRenderer->SetCullDistance(28000.f, 42000.f);
		Sim.RegisterCarcassISM(CarcassRenderer->GetISM());
	}

	// Boid renderers: one instanced mesh per appearance bucket (diet hue x vigour brightness).
	// A creature is drawn in the bucket matching its own genome, so its colour reflects its stats.
	for (int32 Bucket = 0; Bucket < Evo::NumAppearanceBuckets; ++Bucket)
	{
		const int32 Hue = Bucket % Evo::NumDietHues;
		const int32 Shade = Bucket / Evo::NumDietHues;
		if (AEvoswarmSpeciesRenderer* BoidRenderer = World->SpawnActor<AEvoswarmSpeciesRenderer>(AEvoswarmSpeciesRenderer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator))
		{
			BoidRenderer->SetupAppearance(nullptr, Evo::BucketColor(Hue, Shade));
			BoidRenderer->SetCastShadows(bBoidsCastShadows); // GPU lever — the sim isn't the bottleneck, rendering is
			BoidRenderer->SetCullDistance(42000.f, 58000.f); // creatures are the focus — cull farthest
			Sim.RegisterBoidBucketISM(Bucket, BoidRenderer->GetISM());
		}
	}

	// Seed the food field now so plants are visible on frame 1 (the sim tick then maintains
	// it). Seeding goes through the meadow patches too, so the very first frame already shows
	// big natural bunches instead of uniform confetti.
	for (int32 N = 0; N < Evo::FoodTargetCount; ++N)
	{
		FVector FoodPos;
		if (Sim.SamplePlantLocation(FoodPos))
		{
			Sim.SpawnFood(FoodPos);
		}
	}

	for (int32 SpeciesIndex = 0; SpeciesIndex < Species.Num(); ++SpeciesIndex)
	{
		USpeciesConfig* Config = Species[SpeciesIndex];
		if (!Config)
		{
			continue;
		}

		// Species colour is kept only for the HUD/debug overlay; boid bodies are coloured by diet.
		Sim.SetSpeciesInfo(SpeciesIndex, Config->DisplayName, Config->SpeciesColor);

		FBoidSpeciesSharedFragment Shared;
		Shared.SpeciesIndex = SpeciesIndex;
		Shared.Color = Config->SpeciesColor;
		Shared.MeshScale = Config->MeshScale;
		Shared.Nocturnality = Config->Nocturnality;
		Shared.Config = Config;

		for (int32 N = 0; N < Config->SpawnCount; ++N)
		{
			const FBoidGenome Genome = FBoidGenomeLibrary::RandomWithinBudget(*Config, Rng);

			// First spawn scatters every species across the whole arena so they immediately
			// explore (and mix on) the entire map. Subsequent births happen between parents.
			const FVector Pos(
				Rng.FRandRange(-Evo::ArenaHalfExtent, Evo::ArenaHalfExtent),
				Rng.FRandRange(-Evo::ArenaHalfExtent, Evo::ArenaHalfExtent),
				Evo::FlightZ);

			Sim.SpawnBoid(Shared, Genome, Pos);
		}
	}
}

USpeciesConfig* AEvoswarmGameMode::MakeDefaultSpecies(int32 Index)
{
	USpeciesConfig* Cfg = NewObject<USpeciesConfig>(this);

	auto Set = [Cfg](EBoidStat Stat, float Min, float Max, float Cost)
	{
		Cfg->StatDefs.Add(Stat, FBoidStatDef{ Min, Max, Cost });
	};

	// --- Shared baseline (overridden per archetype below) ---
	Set(EBoidStat::HP,               5.f, 15.f, 1.0f);
	Set(EBoidStat::Armor,            0.f, 10.f, 1.0f);
	Set(EBoidStat::WalkSpeed,        4.f, 10.f, 1.0f);
	Set(EBoidStat::RunSpeed,         8.f, 18.f, 1.2f);
	Set(EBoidStat::Stamina,          5.f, 15.f, 0.8f);
	Set(EBoidStat::Regeneration,     1.f,  6.f, 1.0f);
	Set(EBoidStat::Hunger,           6.f, 14.f, 0.7f);
	Set(EBoidStat::Biomass,          3.f, 12.f, 0.5f);
	Set(EBoidStat::Stealth,          0.f, 10.f, 1.0f);
	Set(EBoidStat::Damage,           0.f, 14.f, 1.3f);
	Set(EBoidStat::Intimidation,     0.f, 10.f, 0.6f);
	Set(EBoidStat::Aggressiveness,   0.f, 12.f, 0.8f);
	Set(EBoidStat::Perception,       4.f, 16.f, 1.0f);
	Set(EBoidStat::Diet,             0.f,  0.2f, 0.0f);
	Set(EBoidStat::Lifespan,         6.f, 16.f, 1.0f);
	Set(EBoidStat::ReproductionRate, 1.f,  8.f, 1.0f);
	Set(EBoidStat::Integration,      2.f, 12.f, 0.5f);
	Set(EBoidStat::MutationRate,     0.03f, 0.15f, 0.0f);

	Cfg->Budget = 120.f;
	Cfg->SpawnRegionRadius = 2000.f;

	const float Q = Evo::ArenaHalfExtent * 0.375f; // quadrant offset for the four starting regions
	switch (Index)
	{
	case 0: // Grazer — herbivore herd: cohesive, fast-breeding, harmless
		Cfg->DisplayName = TEXT("Grazer");
		Cfg->SpeciesColor = FLinearColor(0.25f, 0.85f, 0.30f);
		Cfg->MeshScale = 1.4f;
		Cfg->SpawnCount = 360;
		Cfg->MaxPopulation = 620;
		Cfg->SpawnRegionCenter = FVector(Q, Q, Evo::FlightZ);
		Set(EBoidStat::Diet,             0.0f, 0.25f, 0.0f); // herbivore end of the gradient
		Set(EBoidStat::Integration,      6.f, 14.f, 0.5f);
		Set(EBoidStat::ReproductionRate, 7.f, 13.f, 1.0f); // r-strategist: breed fast to outpace predation
		Set(EBoidStat::Damage,           0.f, 3.f, 1.3f);
		break;

	case 1: // Darter — quick, stealthy omnivore-leaning forager
		Cfg->DisplayName = TEXT("Darter");
		Cfg->Nocturnality = 0.15f; // diurnal, slight dusk activity
		Cfg->SpeciesColor = FLinearColor(0.20f, 0.55f, 0.95f);
		Cfg->MeshScale = 1.1f;
		Cfg->SpawnCount = 240;
		Cfg->MaxPopulation = 460;
		Cfg->SpawnRegionCenter = FVector(-Q, Q, Evo::FlightZ);
		Set(EBoidStat::Diet,             0.2f, 0.5f, 0.0f); // herbivore-leaning omnivore
		Set(EBoidStat::WalkSpeed,        7.f, 12.f, 1.0f);
		Set(EBoidStat::RunSpeed,         14.f, 24.f, 1.2f); // fast: outruns predators
		Set(EBoidStat::Stealth,          4.f, 12.f, 1.0f);
		Set(EBoidStat::ReproductionRate, 5.f, 10.f, 1.0f);
		break;

	case 2: // Stalker — agile carnivore, high perception
		Cfg->DisplayName = TEXT("Stalker");
		Cfg->Nocturnality = 1.0f; // hunts by night
		Cfg->SpeciesColor = FLinearColor(0.95f, 0.55f, 0.10f);
		Cfg->MeshScale = 1.5f;
		Cfg->SpawnCount = 150;
		Cfg->MaxPopulation = 340;
		Cfg->SpawnRegionCenter = FVector(-Q, -Q, Evo::FlightZ);
		Set(EBoidStat::Diet,             0.5f, 0.8f, 0.0f); // carnivore-leaning omnivore
		Set(EBoidStat::Perception,       10.f, 18.f, 1.0f);
		Set(EBoidStat::Damage,           6.f, 16.f, 1.3f);
		Set(EBoidStat::Aggressiveness,   5.f, 14.f, 0.8f);
		Set(EBoidStat::ReproductionRate, 3.f, 7.f, 1.0f);
		break;

	default: // Apex — big, durable carnivore, slow to breed
		Cfg->DisplayName = TEXT("Apex");
		Cfg->Nocturnality = 0.8f; // mostly nocturnal
		Cfg->SpeciesColor = FLinearColor(0.9f, 0.15f, 0.15f);
		Cfg->MeshScale = 2.0f;
		Cfg->SpawnCount = 70;
		Cfg->MaxPopulation = 180;
		Cfg->Budget = 150.f;
		Cfg->SpawnRegionCenter = FVector(Q, -Q, Evo::FlightZ);
		Set(EBoidStat::Diet,             0.75f, 1.0f, 0.0f); // carnivore end of the gradient
		Set(EBoidStat::HP,               10.f, 22.f, 1.0f);
		Set(EBoidStat::Damage,           10.f, 20.f, 1.3f);
		Set(EBoidStat::Intimidation,     5.f, 14.f, 0.6f);
		Set(EBoidStat::ReproductionRate, 2.f, 6.f, 1.0f);
		break;
	}

	return Cfg;
}
