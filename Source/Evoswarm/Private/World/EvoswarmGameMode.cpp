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

	// The procedural biome terrain IS the ground (replaces the old flat plane). Always built.
	if (AEvoswarmTerrain* Terrain = World->SpawnActor<AEvoswarmTerrain>(AEvoswarmTerrain::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params))
	{
		Terrain->Build(Evo::ArenaHalfExtent, /*CellsPerSide=*/110);
	}

	// Helper: is there already an actor of this class in the level?
	auto HasActor = [World](UClass* Cls) -> bool
	{
		for (TActorIterator<AActor> It(World, Cls); It; ++It) { return true; }
		return false;
	};

	// --- Sun: high, bright, drives the sky atmosphere (stylized midday) ---
	if (!HasActor(ADirectionalLight::StaticClass()))
	{
		if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector::ZeroVector, FRotator(-52.f, -40.f, 0.f), Params))
		{
			Sun->SetMobility(EComponentMobility::Movable);
			if (UDirectionalLightComponent* C = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
			{
				C->SetIntensity(10.f);
				C->SetLightColor(FLinearColor(1.0f, 0.96f, 0.86f)); // warm sunlight
				C->SetAtmosphereSunLight(true); // this light defines the sky's sun
				C->SetDynamicShadowCascades(4);
			}
		}
	}

	// --- Procedural sky atmosphere (gradient sky + horizon glow, no textures) ---
	if (!HasActor(ASkyAtmosphere::StaticClass()))
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
	if (!HasActor(AVolumetricCloud::StaticClass()))
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
		Sim.RegisterFoodISM(FoodRenderer->GetISM());
	}

	// Carcass renderer: a low-poly lump instanced for every kill.
	UStaticMesh* CarcassMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/EvoGen/Evo_Carcass.Evo_Carcass"));
	if (AEvoswarmSpeciesRenderer* CarcassRenderer = World->SpawnActor<AEvoswarmSpeciesRenderer>(AEvoswarmSpeciesRenderer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator))
	{
		CarcassRenderer->SetupAppearance(CarcassMesh, FLinearColor(0.35f, 0.05f, 0.05f));
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
			Sim.RegisterBoidBucketISM(Bucket, BoidRenderer->GetISM());
		}
	}

	// Seed the food field now so plants are visible on frame 1 (the sim tick then maintains it).
	for (int32 N = 0; N < Evo::FoodTargetCount; ++N)
	{
		const FVector FoodPos(
			Rng.FRandRange(-Evo::ArenaHalfExtent, Evo::ArenaHalfExtent),
			Rng.FRandRange(-Evo::ArenaHalfExtent, Evo::ArenaHalfExtent),
			Evo::FlightZ);
		if (Evo::TerrainHeight(FoodPos.X, FoodPos.Y) < Evo::SeaLevel + Evo::BeachBand)
		{
			continue; // keep plants off the water
		}
		Sim.SpawnFood(FoodPos);
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

	const float Q = 4500.f; // quadrant offset for the four starting regions
	switch (Index)
	{
	case 0: // Grazer — herbivore herd: cohesive, fast-breeding, harmless
		Cfg->DisplayName = TEXT("Grazer");
		Cfg->SpeciesColor = FLinearColor(0.25f, 0.85f, 0.30f);
		Cfg->MeshScale = 1.4f;
		Cfg->SpawnCount = 320;
		Cfg->MaxPopulation = 650;
		Cfg->SpawnRegionCenter = FVector(Q, Q, Evo::FlightZ);
		Set(EBoidStat::Diet,             0.0f, 0.25f, 0.0f); // herbivore end of the gradient
		Set(EBoidStat::Integration,      6.f, 14.f, 0.5f);
		Set(EBoidStat::ReproductionRate, 7.f, 13.f, 1.0f); // r-strategist: breed fast to outpace predation
		Set(EBoidStat::Damage,           0.f, 3.f, 1.3f);
		break;

	case 1: // Darter — quick, stealthy omnivore-leaning forager
		Cfg->DisplayName = TEXT("Darter");
		Cfg->SpeciesColor = FLinearColor(0.20f, 0.55f, 0.95f);
		Cfg->MeshScale = 1.1f;
		Cfg->SpawnCount = 200;
		Cfg->MaxPopulation = 450;
		Cfg->SpawnRegionCenter = FVector(-Q, Q, Evo::FlightZ);
		Set(EBoidStat::Diet,             0.2f, 0.5f, 0.0f); // herbivore-leaning omnivore
		Set(EBoidStat::WalkSpeed,        7.f, 12.f, 1.0f);
		Set(EBoidStat::RunSpeed,         14.f, 24.f, 1.2f); // fast: outruns predators
		Set(EBoidStat::Stealth,          4.f, 12.f, 1.0f);
		Set(EBoidStat::ReproductionRate, 5.f, 10.f, 1.0f);
		break;

	case 2: // Stalker — agile carnivore, high perception
		Cfg->DisplayName = TEXT("Stalker");
		Cfg->SpeciesColor = FLinearColor(0.95f, 0.55f, 0.10f);
		Cfg->MeshScale = 1.5f;
		Cfg->SpawnCount = 35;
		Cfg->MaxPopulation = 120;
		Cfg->SpawnRegionCenter = FVector(-Q, -Q, Evo::FlightZ);
		Set(EBoidStat::Diet,             0.5f, 0.8f, 0.0f); // carnivore-leaning omnivore
		Set(EBoidStat::Perception,       10.f, 18.f, 1.0f);
		Set(EBoidStat::Damage,           6.f, 16.f, 1.3f);
		Set(EBoidStat::Aggressiveness,   5.f, 14.f, 0.8f);
		Set(EBoidStat::ReproductionRate, 1.f, 5.f, 1.0f);
		break;

	default: // Apex — big, durable carnivore, slow to breed
		Cfg->DisplayName = TEXT("Apex");
		Cfg->SpeciesColor = FLinearColor(0.9f, 0.15f, 0.15f);
		Cfg->MeshScale = 2.0f;
		Cfg->SpawnCount = 14;
		Cfg->MaxPopulation = 55;
		Cfg->Budget = 150.f;
		Cfg->SpawnRegionCenter = FVector(Q, -Q, Evo::FlightZ);
		Set(EBoidStat::Diet,             0.75f, 1.0f, 0.0f); // carnivore end of the gradient
		Set(EBoidStat::HP,               10.f, 22.f, 1.0f);
		Set(EBoidStat::Damage,           10.f, 20.f, 1.3f);
		Set(EBoidStat::Intimidation,     5.f, 14.f, 0.6f);
		Set(EBoidStat::ReproductionRate, 1.f, 4.f, 1.0f);
		break;
	}

	return Cfg;
}
