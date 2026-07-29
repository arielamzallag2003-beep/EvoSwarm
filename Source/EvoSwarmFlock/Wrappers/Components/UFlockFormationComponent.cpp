#include "Wrappers/Components/UFlockFormationComponent.h"
#include "Wrappers/Components/UFlockManagerComponent.h"
#include "Engine/World.h"

UFlockFormationComponent::UFlockFormationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UFlockFormationComponent::BeginPlay()
{
    Super::BeginPlay();
    // Auto-detect FlockIndex from sibling UFlockManagerComponent
    if (FlockIndex == INDEX_NONE)
    {
        if (UFlockManagerComponent* Mgr = GetOwner()->FindComponentByClass<UFlockManagerComponent>())
            FlockIndex = Mgr->GetFlockIndex();
    }
}

FFlock* UFlockFormationComponent::GetFlock() const
{
    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    return Sub ? Sub->GetFlock(FlockIndex) : nullptr;
}

void UFlockFormationComponent::Activate(bool bReset)
{
    Super::Activate(bReset);
    FFlock* Flock = GetFlock();
    if (!Flock) return;

    Flock->Formation.Type        = FormationType;
    Flock->Formation.Spacing     = Spacing;
    Flock->Formation.LeaderIndex = LeaderBoidIndex;
    Flock->Formation.bIsActive   = true;
    Flock->Formation.bSlotsDirty = true;
}

void UFlockFormationComponent::Deactivate()
{
    if (FFlock* Flock = GetFlock())
        Flock->Formation.bIsActive = false;
    Super::Deactivate();
}

void UFlockFormationComponent::SetFormationType(EFormationType NewType)
{
    FormationType = NewType;
    if (FFlock* Flock = GetFlock())
    {
        Flock->Formation.Type       = NewType;
        Flock->Formation.bSlotsDirty = true;
    }
}

void UFlockFormationComponent::SetLeader(int32 NewLeaderIndex)
{
    LeaderBoidIndex = NewLeaderIndex;
    if (FFlock* Flock = GetFlock())
    {
        Flock->Formation.LeaderIndex = NewLeaderIndex;
        Flock->Formation.bSlotsDirty = true;
    }
}
