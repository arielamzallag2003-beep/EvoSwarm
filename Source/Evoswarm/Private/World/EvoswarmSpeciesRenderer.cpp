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
	ISM->SetCastShadow(true);
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
