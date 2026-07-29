// Copyright Evoswarm.
//
// Builds the visible terrain as a procedural mesh sampled from Evo::TerrainHeight, split
// into one colour-tinted section per biome (Evo::BiomeAt).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EvoswarmTerrainActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS()
class EVOSWARM_API AEvoswarmTerrain : public AActor
{
	GENERATED_BODY()

public:
	AEvoswarmTerrain();

	/** Generate the mesh covering [-HalfExtent,+HalfExtent] in X/Y with CellsPerSide quads. */
	void Build(float HalfExtent, int32 CellsPerSide);

private:
	void ScatterFlora(float HalfExtent);
	void BuildWater(float HalfExtent, UMaterialInterface* BaseMat);

	UPROPERTY(VisibleAnywhere, Category = "Evoswarm")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	FRandomStream Rng = FRandomStream(2024);
};
