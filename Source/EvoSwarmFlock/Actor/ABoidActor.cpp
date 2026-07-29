#include "Actor/ABoidActor.h"
#include "Flock/FlockSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ABoidActor::ABoidActor()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    SetRootComponent(MeshComp);
}

void ABoidActor::BeginPlay()
{
    Super::BeginPlay();
    CachedSubsystem = GetWorld()->GetSubsystem<UFlockSubsystem>();
}

void ABoidActor::BindToBoid(int32 InFlockIndex, int32 InBoidIndex)
{
    FlockIndex = InFlockIndex;
    BoidIndex  = InBoidIndex;
}

void ABoidActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!CachedSubsystem.IsValid() || FlockIndex == INDEX_NONE || BoidIndex == INDEX_NONE)
        return;

    FVector Position, Forward;
    if (CachedSubsystem->GetBoidTransform(FlockIndex, BoidIndex, Position, Forward))
    {
        SetActorLocation(Position);
        if (!Forward.IsNearlyZero())
            SetActorRotation(Forward.Rotation());
    }
}
