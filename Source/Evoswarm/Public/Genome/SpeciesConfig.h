// Copyright Evoswarm.
//
// Designer-authored definition of one species: its stat ranges + costs, total point
// budget, look, and how/where its starting population spawns. One asset per species;
// the first scene uses four.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BoidStats.h"
#include "SpeciesConfig.generated.h"

class UStaticMesh;

UCLASS(BlueprintType)
class EVOSWARM_API USpeciesConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USpeciesConfig();

	/** Friendly name shown in logs / debug. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FString DisplayName = TEXT("Species");

	/** Instance tint, applied via per-instance custom data on the ISM. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FLinearColor SpeciesColor = FLinearColor::White;

	/** Render mesh for this species' instanced static mesh. Falls back to an engine sphere if null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Uniform render scale of each instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	float MeshScale = 1.f;

	/** Temporal niche: 0 = diurnal (active by day, sleeps at night), 1 = nocturnal,
	 *  0.5 = cathemeral. Drives perception, fatigue and roosting along the day/night cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Nocturnality = 0.f;

	/** Total stat-point budget. Sum(value * CostWeight) across stats must stay <= this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution", meta = (ClampMin = "0.0"))
	float Budget = 100.f;

	/** Per-stat Min / Max / CostWeight. Missing entries use a neutral default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Evolution")
	TMap<EBoidStat, FBoidStatDef> StatDefs;

	/** How many individuals to spawn at start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0"))
	int32 SpawnCount = 150;

	/** Hard population ceiling: reproduction stops once the species reaches this count (carrying
	 *  capacity). Bounds entity count for performance without altering any behaviour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "1"))
	int32 MaxPopulation = 400;

	/** Centre of this species' starting region (world space). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
	FVector SpawnRegionCenter = FVector::ZeroVector;

	/** Radius of the starting region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (ClampMin = "0.0"))
	float SpawnRegionRadius = 5000.f;

	/** Returns the stat definition, or a neutral default (Min 0, Max 1, Cost 1) if unset. */
	FBoidStatDef GetStatDef(EBoidStat Stat) const;
};
