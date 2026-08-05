// Copyright Evoswarm.

#include "EvoswarmTerrainActor.h"
#include "EvoswarmTerrain.h"
#include "EvoswarmTuning.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// Surface layers: a coloured terrain section each. Shorelines get sand, steep ground gets
	// bare rock, and highland peaks get snow — so the land reads as a real landscape.
	enum class ESurface : uint8 { Grassland, Forest, Desert, HighlandRock, HighlandSnow, Beach, Cliff, MAX };

	ESurface SurfaceAt(float X, float Y, float Height)
	{
		// Shoreline sand: a band just above (and the ground below) the waterline.
		if (Height < Evo::SeaLevel + Evo::BeachBand)
		{
			return ESurface::Beach;
		}
		// Steep faces show exposed rock regardless of biome.
		if (Evo::TerrainNormal(X, Y).Z < Evo::CliffNormalZ)
		{
			return ESurface::Cliff;
		}
		switch (Evo::BiomeAt(X, Y))
		{
		case EBiome::Grassland: return ESurface::Grassland;
		case EBiome::Forest:    return ESurface::Forest;
		case EBiome::Desert:    return ESurface::Desert;
		default:                return Height >= Evo::SnowLine ? ESurface::HighlandSnow : ESurface::HighlandRock;
		}
	}

	FLinearColor SurfaceColor(ESurface S)
	{
		switch (S)
		{
		case ESurface::Grassland:    return Evo::GetBiomeParams(EBiome::Grassland).Color;
		case ESurface::Forest:       return Evo::GetBiomeParams(EBiome::Forest).Color;
		case ESurface::Desert:       return Evo::GetBiomeParams(EBiome::Desert).Color;
		case ESurface::HighlandRock: return Evo::GetBiomeParams(EBiome::Highlands).Color;
		case ESurface::HighlandSnow: return FLinearColor(0.90f, 0.93f, 0.97f); // snow
		case ESurface::Beach:        return FLinearColor(0.80f, 0.72f, 0.52f); // wet sand
		case ESurface::Cliff:        return FLinearColor(0.30f, 0.27f, 0.25f); // bare rock
		default:                     return FLinearColor::White;
		}
	}
}

AEvoswarmTerrain::AEvoswarmTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = Mesh;
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->bUseAsyncCooking = true;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // boids follow height analytically
}

void AEvoswarmTerrain::Build(float HalfExtent, int32 CellsPerSide)
{
	if (!Mesh || CellsPerSide < 1)
	{
		return;
	}

	const float Cell = (HalfExtent * 2.f) / static_cast<float>(CellsPerSide);
	const float UVScale = 1.f / 1000.f;
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	struct FLayer
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TArray<FProcMeshTangent> Tangents;
	};
	const int32 NumLayers = static_cast<int32>(ESurface::MAX);
	TArray<FLayer> MeshLayers;
	MeshLayers.SetNum(NumLayers);

	// Single pass: classify each quad into its surface layer.
	for (int32 IY = 0; IY < CellsPerSide; ++IY)
	{
		for (int32 IX = 0; IX < CellsPerSide; ++IX)
		{
			const float X0 = -HalfExtent + IX * Cell;
			const float Y0 = -HalfExtent + IY * Cell;
			const float X1 = X0 + Cell;
			const float Y1 = Y0 + Cell;
			const float CX = X0 + Cell * 0.5f;
			const float CY = Y0 + Cell * 0.5f;

			const ESurface Surface = SurfaceAt(CX, CY, Evo::TerrainHeight(CX, CY));
			FLayer& L = MeshLayers[static_cast<int32>(Surface)];
			const FLinearColor Col = SurfaceColor(Surface);

			const int32 Base = L.Verts.Num();
			const float Corners[4][2] = { {X0, Y0}, {X1, Y0}, {X0, Y1}, {X1, Y1} };
			for (int32 C = 0; C < 4; ++C)
			{
				const float VX = Corners[C][0];
				const float VY = Corners[C][1];
				L.Verts.Add(FVector(VX, VY, Evo::TerrainHeight(VX, VY)));
				L.Normals.Add(Evo::TerrainNormal(VX, VY));
				L.UVs.Add(FVector2D(VX * UVScale, VY * UVScale));
				L.Colors.Add(Col);
				L.Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
			}
			L.Tris.Add(Base + 0); L.Tris.Add(Base + 2); L.Tris.Add(Base + 1);
			L.Tris.Add(Base + 1); L.Tris.Add(Base + 2); L.Tris.Add(Base + 3);
		}
	}

	for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FLayer& L = MeshLayers[LayerIndex];
		if (L.Verts.Num() == 0)
		{
			continue;
		}
		Mesh->CreateMeshSection_LinearColor(LayerIndex, L.Verts, L.Tris, L.Normals, L.UVs, L.Colors, L.Tangents, /*bCreateCollision=*/false);
		if (BaseMat)
		{
			UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(BaseMat, this);
			Dyn->SetVectorParameterValue(TEXT("Color"), SurfaceColor(static_cast<ESurface>(LayerIndex)));
			Mesh->SetMaterial(LayerIndex, Dyn);
		}
	}

	BuildWater(HalfExtent, BaseMat);
	ScatterFlora(HalfExtent);
}

void AEvoswarmTerrain::BuildWater(float HalfExtent, UMaterialInterface* BaseMat)
{
	// A low-poly faceted water plane at sea level; basins below it read as lakes and sea.
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/EvoGen/Evo_Water.Evo_Water"));
	if (!Plane)
	{
		Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}
	if (!Plane)
	{
		return;
	}

	UStaticMeshComponent* Water = NewObject<UStaticMeshComponent>(this);
	Water->SetupAttachment(RootComponent);
	Water->RegisterComponent();
	Water->SetMobility(EComponentMobility::Movable);
	Water->SetStaticMesh(Plane);
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetCanEverAffectNavigation(false);

	// Evo_Water is ~2 m across and imports Y-up; stand it flat and scale it over the whole arena.
	const float Scale = (HalfExtent * 2.f * 1.04f) / 200.f;
	Water->SetWorldRotation(Evo::MeshStandUp());
	Water->SetWorldScale3D(FVector(Scale));
	Water->SetWorldLocation(FVector(0.f, 0.f, Evo::SeaLevel));

	// Prefer the translucent, glossy water material; fall back to a flat blue tint if it's missing.
	if (UMaterialInterface* WaterMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/EvoGen/M_Water.M_Water")))
	{
		Water->SetMaterial(0, WaterMat);
	}
	else if (BaseMat)
	{
		UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(BaseMat, this);
		Tint->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.09f, 0.32f, 0.46f));
		Water->SetMaterial(0, Tint);
	}
	AddInstanceComponent(Water);
}

void AEvoswarmTerrain::ScatterFlora(float HalfExtent)
{
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	auto Load = [](const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path); };
	UStaticMesh* PineA   = Load(TEXT("/Game/EvoGen/Evo_Pine_A.Evo_Pine_A"));
	UStaticMesh* PineB   = Load(TEXT("/Game/EvoGen/Evo_Pine_B.Evo_Pine_B"));
	UStaticMesh* RockA   = Load(TEXT("/Game/EvoGen/Evo_Rock_A.Evo_Rock_A"));
	UStaticMesh* RockB   = Load(TEXT("/Game/EvoGen/Evo_Rock_B.Evo_Rock_B"));
	UStaticMesh* RockBig = Load(TEXT("/Game/EvoGen/Evo_RockBig.Evo_RockBig"));
	UStaticMesh* BushM   = Load(TEXT("/Game/EvoGen/Evo_Bush.Evo_Bush"));
	UStaticMesh* CactusM = Load(TEXT("/Game/EvoGen/Evo_Cactus.Evo_Cactus"));
	UStaticMesh* FlowerM = Load(TEXT("/Game/EvoGen/Evo_Flower.Evo_Flower"));

	auto MakeProp = [this, BaseMat](UStaticMesh* M, const FLinearColor& Col) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
		ISM->SetupAttachment(RootComponent);
		ISM->RegisterComponent();
		ISM->SetMobility(EComponentMobility::Static);
		ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ISM->SetCanEverAffectNavigation(false);
		if (M)
		{
			ISM->SetStaticMesh(M);
		}
		if (BaseMat)
		{
			UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(BaseMat, this);
			Dyn->SetVectorParameterValue(TEXT("Color"), Col);
			ISM->SetMaterial(0, Dyn);
		}
		AddInstanceComponent(ISM);
		return ISM;
	};

	UInstancedStaticMeshComponent* PinesDark  = MakeProp(PineA,   FLinearColor(0.06f, 0.22f, 0.08f));
	UInstancedStaticMeshComponent* PinesLight = MakeProp(PineB,   FLinearColor(0.12f, 0.34f, 0.12f));
	UInstancedStaticMeshComponent* Rocks      = MakeProp(RockA,   FLinearColor(0.40f, 0.39f, 0.42f));
	UInstancedStaticMeshComponent* RocksAng   = MakeProp(RockB,   FLinearColor(0.34f, 0.32f, 0.34f));
	UInstancedStaticMeshComponent* BigRocks   = MakeProp(RockBig, FLinearColor(0.38f, 0.36f, 0.40f));
	UInstancedStaticMeshComponent* SnowRocks  = MakeProp(RockA,   FLinearColor(0.88f, 0.90f, 0.95f));
	UInstancedStaticMeshComponent* Cacti      = MakeProp(CactusM, FLinearColor(0.16f, 0.42f, 0.22f));
	UInstancedStaticMeshComponent* Bushes     = MakeProp(BushM,   FLinearColor(0.20f, 0.46f, 0.16f));
	UInstancedStaticMeshComponent* Flowers    = MakeProp(FlowerM, FLinearColor(0.95f, 0.80f, 0.22f));

	// Stand the (Y-up) meshes upright, add a random yaw, and place them. Base-pivot meshes (trees,
	// cactus, flowers) sit on the ground; centre-pivot meshes (rocks, bushes) get a small lift.
	auto Place = [this](UInstancedStaticMeshComponent* ISM, float X, float Y, float Z, float Scale, float Lift)
	{
		const FQuat Rot = FQuat(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f)) * Evo::MeshStandUp();
		ISM->AddInstance(FTransform(Rot, FVector(X, Y, Z + Lift * Scale), FVector(Scale)), /*bWorldSpace=*/true);
	};

	const int32 Samples = 9000;
	for (int32 I = 0; I < Samples; ++I)
	{
		const float X = Rng.FRandRange(-HalfExtent, HalfExtent);
		const float Y = Rng.FRandRange(-HalfExtent, HalfExtent);
		const float Z = Evo::TerrainHeight(X, Y);
		if (Z < Evo::SeaLevel + Evo::BeachBand)
		{
			continue; // keep props out of the water and off the bare sand
		}

		switch (Evo::BiomeAt(X, Y))
		{
		case EBiome::Forest:
			if (Rng.FRand() < 0.8f)
			{
				Place((Rng.FRand() < 0.5f) ? PinesDark : PinesLight, X, Y, Z, Rng.FRandRange(1.0f, 2.0f), 0.f);
			}
			break;
		case EBiome::Highlands:
			if (Rng.FRand() < 0.13f)
			{
				const float S = Rng.FRandRange(0.7f, 1.8f);
				if (Z >= Evo::SnowLine)        Place(SnowRocks, X, Y, Z, S, 30.f);
				else if (Rng.FRand() < 0.3f)   Place(BigRocks,  X, Y, Z, S, 50.f);
				else                           Place((Rng.FRand() < 0.5f) ? Rocks : RocksAng, X, Y, Z, S, 30.f);
			}
			break;
		case EBiome::Desert:
			if (Rng.FRand() < 0.07f)
			{
				Place(Cacti, X, Y, Z, Rng.FRandRange(1.0f, 1.8f), 0.f);
			}
			else if (Rng.FRand() < 0.04f)
			{
				Place((Rng.FRand() < 0.5f) ? Rocks : RocksAng, X, Y, Z, Rng.FRandRange(0.7f, 1.4f), 28.f);
			}
			break;
		default: // Grassland
			if (Rng.FRand() < 0.20f)
			{
				Place(Bushes, X, Y, Z, Rng.FRandRange(0.5f, 1.0f), 22.f);
			}
			else if (Rng.FRand() < 0.06f)
			{
				Place(Flowers, X, Y, Z, Rng.FRandRange(0.5f, 1.0f), 0.f);
			}
			break;
		}
	}
}
