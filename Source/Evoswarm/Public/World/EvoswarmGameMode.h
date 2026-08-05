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

private:
	void CleanScene();
	void BuildEnvironment();
	void SpawnSpecies(UEvoswarmSimSubsystem& Sim);
	USpeciesConfig* MakeDefaultSpecies(int32 Index);

	FRandomStream Rng = FRandomStream(7);
};
