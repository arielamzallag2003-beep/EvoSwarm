#include "Wrappers/Components/UBoidCommandComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

UBoidCommandComponent::UBoidCommandComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UBoidCommandComponent::BeginPlay()
{
    Super::BeginPlay();
    // Try to auto-bind from a sibling ABoidActor
    CreateHighlightMesh();
}

void UBoidCommandComponent::BindToBoid(int32 InFlockIndex, int32 InBoidIndex)
{
    FlockIndex = InFlockIndex;
    BoidIndex  = InBoidIndex;
}

FBoidData* UBoidCommandComponent::GetBoidData() const
{
    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (!Sub) return nullptr;
    FFlock* Flock = Sub->GetFlock(FlockIndex);
    if (!Flock || !Flock->Boids.IsValidIndex(BoidIndex)) return nullptr;
    return &Flock->Boids[BoidIndex];
}

void UBoidCommandComponent::GiveMoveOrder(FVector Destination)
{
    if (FBoidData* Boid = GetBoidData())
    {
        Boid->CommandTarget     = Destination;
        Boid->bHasCommandTarget = true;
    }
}

void UBoidCommandComponent::Stop()
{
    if (FBoidData* Boid = GetBoidData())
        Boid->bHasCommandTarget = false;
}

void UBoidCommandComponent::SetSelected(bool bSelected)
{
    bIsSelected = bSelected;
    if (HighlightMesh)
        HighlightMesh->SetVisibility(bSelected);
}

void UBoidCommandComponent::CreateHighlightMesh()
{
    // Procedural flat cylinder disc under the boid, mirrors Unity's sphere highlight
    UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!CylinderMesh) return;

    HighlightMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("SelectionHighlight"));
    HighlightMesh->SetStaticMesh(CylinderMesh);
    HighlightMesh->SetRelativeLocation(FVector(0.f, 0.f, -45.f));
    HighlightMesh->SetRelativeScale3D(FVector(HighlightRadius / 50.f, HighlightRadius / 50.f, 0.08f));
    HighlightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HighlightMesh->SetVisibility(false);
    HighlightMesh->RegisterComponent();
    HighlightMesh->AttachToComponent(GetOwner()->GetRootComponent(),
                                     FAttachmentTransformRules::KeepRelativeTransform);

    // Translucent highlight material
    UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(
        LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")),
        HighlightMesh);
    if (Mat)
    {
        Mat->SetVectorParameterValue(TEXT("Color"), HighlightColor);
        HighlightMesh->SetMaterial(0, Mat);
    }
}
