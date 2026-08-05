// Copyright Evoswarm.
//
// One of these per species. Holds the InstancedStaticMeshComponent the render processor
// writes every frame, and tints it with the species colour.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EvoswarmSpeciesRenderer.generated.h"

class UInstancedStaticMeshComponent;
class USpeciesConfig;

UCLASS()
class EVOSWARM_API AEvoswarmSpeciesRenderer : public AActor
{
	GENERATED_BODY()

public:
	AEvoswarmSpeciesRenderer();

	/** Pick the mesh, scale and colour for this species. Safe to call with a null config (uses defaults). */
	void Setup(const USpeciesConfig* Config);

	/** Configure the instanced mesh directly (used for the food renderer). Null mesh -> engine sphere. */
	void SetupAppearance(UStaticMesh* Mesh, const FLinearColor& Color);

	UInstancedStaticMeshComponent* GetISM() const { return ISM; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Evoswarm")
	TObjectPtr<UInstancedStaticMeshComponent> ISM;
};
