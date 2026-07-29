// Copyright Evoswarm.
//
// Turnkey entry point. On BeginPlay it (optionally) builds lighting + a floor, then
// spawns the four species into their own regions. Assign authored USpeciesConfig assets
// in the Species array, or leave it empty to get four generated default species so the
// scene runs out of the box.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EvoswarmGameMode.generated.h"

class USpeciesConfig;
class UEvoswarmSimSubsystem;

UCLASS()
class EVOSWARM_API AEvoswarmGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEvoswarmGameMode();

	virtual void BeginPlay() override;

	/** Authored species. If empty, four defaults are generated at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evoswarm")
	TArray<TObjectPtr<USpeciesConfig>> Species;

	/** Spawn a sun, sky light and floor so even an empty level is lit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evoswarm")
	bool bBuildEnvironment = true;

	/** On Play, destroy pre-existing lighting/sky/fog/landscape/mesh actors so the procedural
	 *  scene starts from a clean slate (no need to delete level clutter by hand). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evoswarm")
	bool bCleanSceneOnPlay = true;

	/** Whether the creatures cast dynamic shadows. The sim itself is cheap (~2 ms) — the frame
	 *  cost is GPU/rendering, and ~1500 shadow-casting creatures under the moving sun are a big
	 *  part of it. Turn OFF for a large FPS gain at large populations (creatures lose their
	 *  ground shadow). Plants/carcasses never cast shadows regardless. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evoswarm|Performance")
	bool bBoidsCastShadows = true;

	/** Use the GoodSky pack (Content/GoodSky) for the sky dome + automatic day/night cycle.
	 *  Falls back to the engine SkyAtmosphere + volumetric clouds if the pack is missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evoswarm|Sky")
	bool bUseGoodSky = true;

	/** Hour the simulation starts at (0 = midnight, 6 = sunrise, 12 = noon, 18 = sunset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evoswarm|Sky", meta = (ClampMin = "0.0", ClampMax = "24.0", EditCondition = "bUseGoodSky"))
	float GoodSkyStartHour = 9.f;

	/** Real-time minutes one full in-game day lasts (GoodSky auto cycle speed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evoswarm|Sky", meta = (ClampMin = "0.1", EditCondition = "bUseGoodSky"))
	float GoodSkyDayLengthMinutes = 15.f;

private:
	void CleanScene();
	void BuildEnvironment();
	/** Spawns BP_GoodSky and wires it (by reflection, no hard dependency) to drive the sun. */
	AActor* SpawnGoodSky(class ADirectionalLight* Sun);
	void SpawnSpecies(UEvoswarmSimSubsystem& Sim);
	USpeciesConfig* MakeDefaultSpecies(int32 Index);

	/** Repaints the height fog to follow the sun (GoodSky animates the sun; the fog is static,
	 *  and an untouched pale-blue fog band around the horizon broke dusk/night everywhere). */
	void UpdateFogWithSky();
	TWeakObjectPtr<class ADirectionalLight> SunLight;
	TWeakObjectPtr<class AExponentialHeightFog> FogActor;
	TWeakObjectPtr<AActor> GoodSkyActor;
	FTimerHandle FogSyncTimer;

	/** C++ owns the day clock (GoodSky's auto cycle ignores the start hour): current hour,
	 *  the world-seconds the clock started, and the BP refresh function to invoke after
	 *  pushing the hour into its "custom mode" variable. */
	double SkyTimeOfDay = 9.0;
	double SkyClockStartSeconds = 0.0;
	FName GoodSkyUpdateFunc = NAME_None;

	FRandomStream Rng = FRandomStream(7);
};
