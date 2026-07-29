// Copyright Evoswarm.

#include "EvoswarmSpeciesRenderer.h"
#include "SpeciesConfig.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AEvoswarmSpeciesRenderer::AEvoswarmSpeciesRenderer()
{
	PrimaryActorTick.bCanEverTick = false;

	ISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISM"));
	RootComponent = ISM;
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCanEverAffectNavigation(false);
	ISM->NumCustomDataFloats = 0;
	// Shadows are the single biggest GPU cost here: with ~10k instances under a moving sun
	// and 4 CSM cascades the shadow pass dominates the frame. Off by default; the game mode
	// re-enables it only for the (few) boid ISMs. These instances also don't need to feed
	// Lumen/distance-field lighting.
	ISM->SetCastShadow(false);
	ISM->bAffectDynamicIndirectLighting = false;
	ISM->bAffectDistanceFieldLighting = false;
	ISM->SetReceivesDecals(false);
}

void AEvoswarmSpeciesRenderer::SetCastShadows(bool bEnabled)
{
	if (ISM)
	{
		ISM->SetCastShadow(bEnabled);
	}
}

void AEvoswarmSpeciesRenderer::SetCullDistance(float Start, float End)
{
	if (ISM)
	{
		// Per-instance cull: fades between Start and End, culled beyond End (base pass AND shadows).
		ISM->SetCullDistances(static_cast<int32>(Start), static_cast<int32>(End));
	}
}

void AEvoswarmSpeciesRenderer::Setup(const USpeciesConfig* Config)
{
	UStaticMesh* Mesh = (Config && Config->Mesh) ? Config->Mesh : nullptr;
	const FLinearColor Color = Config ? Config->SpeciesColor : FLinearColor::White;
	SetupAppearance(Mesh, Color);
}

void AEvoswarmSpeciesRenderer::SetupAppearance(UStaticMesh* Mesh, const FLinearColor& Color)
{
	if (!Mesh)
	{
		// Engine primitive fallback so the scene works before any art is imported.
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	}
	if (Mesh)
	{
		ISM->SetStaticMesh(Mesh);
	}

	// Tint via a dynamic instance of the engine BasicShapeMaterial (exposes a "Color" param).
	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(BaseMat, this);
		Dyn->SetVectorParameterValue(TEXT("Color"), Color);
		ISM->SetMaterial(0, Dyn);
	}
}
