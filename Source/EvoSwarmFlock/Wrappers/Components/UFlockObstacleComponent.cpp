#include "Wrappers/Components/UFlockObstacleComponent.h"
#include "Engine/World.h"

UFlockObstacleComponent::UFlockObstacleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

float UFlockObstacleComponent::GetScaledRadius() const
{
    FVector Scale = GetOwner()->GetActorScale3D();
    float MaxScale = FMath::Max3(Scale.X, Scale.Y, Scale.Z);
    return BaseRadius * MaxScale;
}

void UFlockObstacleComponent::BeginPlay()
{
    Super::BeginPlay();
    RegisterWithFlocks();
}

void UFlockObstacleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterFromFlocks();
    Super::EndPlay(EndPlayReason);
}

void UFlockObstacleComponent::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Func)
{
    Super::TickComponent(DeltaTime, TickType, Func);
    UpdateFlockObstacles();
}

void UFlockObstacleComponent::RegisterWithFlocks()
{
    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (!Sub) return;

    FObstacleData Data;
    Data.Position  = GetOwner()->GetActorLocation();
    Data.Radius    = GetScaledRadius();
    Data.bIsActive = bObstacleActive;

    // Register with specific flocks or all of them
    auto DoRegister = [&](int32 FlockIdx)
    {
        FFlock* Flock = Sub->GetFlock(FlockIdx);
        if (!Flock) return;
        int32 Slot = Flock->Obstacles.Add(Data);
        ObstacleSlots.Add(FlockIdx, Slot);
    };

    if (TargetFlockIndices.Num() > 0)
    {
        for (int32 Idx : TargetFlockIndices)
            DoRegister(Idx);
    }
    else
    {
        for (int32 i = 0; i < Sub->AllFlocks.Num(); ++i)
            DoRegister(i);
    }
}

void UFlockObstacleComponent::UpdateFlockObstacles()
{
    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (!Sub) return;

    FVector  WorldPos = GetOwner()->GetActorLocation();
    float    ScaledR  = GetScaledRadius();

    for (auto& [FlockIdx, Slot] : ObstacleSlots)
    {
        FFlock* Flock = Sub->GetFlock(FlockIdx);
        if (!Flock || !Flock->Obstacles.IsValidIndex(Slot)) continue;
        Flock->Obstacles[Slot].Position  = WorldPos;
        Flock->Obstacles[Slot].Radius    = ScaledR;
        Flock->Obstacles[Slot].bIsActive = bObstacleActive;
    }
}

void UFlockObstacleComponent::UnregisterFromFlocks()
{
    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (!Sub) return;

    for (auto& [FlockIdx, Slot] : ObstacleSlots)
    {
        if (FFlock* Flock = Sub->GetFlock(FlockIdx))
        {
            if (Flock->Obstacles.IsValidIndex(Slot))
                Flock->Obstacles[Slot].bIsActive = false;
        }
    }
    ObstacleSlots.Reset();
}
