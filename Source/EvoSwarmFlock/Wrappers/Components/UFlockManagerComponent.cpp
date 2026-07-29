#include "Wrappers/Components/UFlockManagerComponent.h"
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"

UFlockManagerComponent::UFlockManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  BeginPlay
// ─────────────────────────────────────────────────────────────────────────────
void UFlockManagerComponent::BeginPlay()
{
    Super::BeginPlay();

    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (!Sub) return;

    // Create the flock
    FlockIndex = Sub->CreateFlock(FlockId);
    FFlock* Flock = Sub->GetFlock(FlockIndex);
    if (!Flock) return;

    // Push settings
    PushSettingsToFlock(*Flock);

    // Spawn boids
    SpawnBoids();
}

// ─────────────────────────────────────────────────────────────────────────────
//  EndPlay — destroy the flock so it doesn't persist between PIE runs
// ─────────────────────────────────────────────────────────────────────────────
void UFlockManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>())
    {
        Sub->DestroyFlock(FlockIndex);
    }
    SpawnedBoids.Empty();
    FlockIndex = INDEX_NONE;
    Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tick — update anchor position if needed
// ─────────────────────────────────────────────────────────────────────────────
void UFlockManagerComponent::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Func)
{
    Super::TickComponent(DeltaTime, TickType, Func);

    if (!bAnchorFollowsOwner) return;
    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (FFlock* Flock = Sub ? Sub->GetFlock(FlockIndex) : nullptr)
        Flock->AnchorPosition = GetOwner()->GetActorLocation();
}

// ─────────────────────────────────────────────────────────────────────────────
//  PushSettingsToFlock
// ─────────────────────────────────────────────────────────────────────────────
void UFlockManagerComponent::PushSettingsToFlock(FFlock& Flock)
{
    // Timing
    Flock.bUseFixedTimestep = bUseFixedTimestep;
    Flock.FixedTimestep     = FixedTimestep;
    Flock.AnchorPosition    = GetOwner()->GetActorLocation();

    // Settings template (index 0 = default)
    Flock.SettingsTemplates.Reset();
    if (BoidSettings)
        Flock.SettingsTemplates.Add(BoidSettings->ToStruct());
    else
        Flock.SettingsTemplates.AddDefaulted();

    // Default behaviour stack
    if (DefaultBehaviourStack)
    {
        Flock.DefaultBehaviours    = DefaultBehaviourStack->BuildStack();
        Flock.bDefaultBehavioursDirty = true;
    }

    // State machine
    if (StateMachineSetup)
        StateMachineSetup->ApplyToFlock(Flock);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SpawnBoids
//  Mirrors FlockManager.cs Awake spawn loop
// ─────────────────────────────────────────────────────────────────────────────
void UFlockManagerComponent::SpawnBoids()
{
    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (!Sub || FlockIndex == INDEX_NONE) return;

    FFlock* Flock = Sub->GetFlock(FlockIndex);
    if (!Flock) return;

    EMovementPlane Plane = (BoidSettings) ? BoidSettings->MovementPlane : EMovementPlane::XZ;
    FVector Origin = GetOwner()->GetActorLocation();

    TSubclassOf<ABoidActor> ClassToSpawn = BoidActorClass ? BoidActorClass : TSubclassOf<ABoidActor>(ABoidActor::StaticClass());

    for (int32 i = 0; i < SpawnCount; ++i)
    {
        // Random position matching the movement plane
        FVector RandOffset;
        switch (Plane)
        {
        case EMovementPlane::XY:
        {
            FVector2D C = FMath::RandPointInCircle(SpawnRadius);
            RandOffset = FVector(C.X, C.Y, 0.f);
            break;
        }
        case EMovementPlane::XZ:
        {
            FVector2D C = FMath::RandPointInCircle(SpawnRadius);
            RandOffset = FVector(C.X, 0.f, C.Y);
            break;
        }
        default:
            RandOffset = FMath::VRand() * FMath::RandRange(0.f, SpawnRadius);
            break;
        }

        FVector SpawnPos = Origin + RandOffset;
        FRotator SpawnRot = FRotator(0.f, FMath::RandRange(0.f, 360.f), 0.f);

        FActorSpawnParameters Params;
        Params.Owner = GetOwner();
        ABoidActor* BoidActor = GetWorld()->SpawnActor<ABoidActor>(ClassToSpawn, SpawnPos, SpawnRot, Params);
        if (!BoidActor) continue;

        // Create initial boid data
        FBoidData InitData;
        InitData.Position = SpawnPos;
        InitData.Forward  = SpawnRot.Vector();
        InitData.bIsActive = 1;

        int32 BoidIdx = Sub->AddBoidToFlock(FlockIndex, InitData);
        BoidActor->BindToBoid(FlockIndex, BoidIdx);

        SpawnedBoids.Add(BoidActor);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Dynamic registration
// ─────────────────────────────────────────────────────────────────────────────
int32 UFlockManagerComponent::RegisterBoid(ABoidActor* Actor)
{
    if (!Actor) return INDEX_NONE;
    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (!Sub) return INDEX_NONE;

    FBoidData InitData;
    InitData.Position  = Actor->GetActorLocation();
    InitData.Forward   = Actor->GetActorForwardVector();
    InitData.bIsActive = 1;

    int32 Idx = Sub->AddBoidToFlock(FlockIndex, InitData);
    Actor->BindToBoid(FlockIndex, Idx);
    SpawnedBoids.AddUnique(Actor);
    return Idx;
}

void UFlockManagerComponent::UnregisterBoid(ABoidActor* Actor)
{
    if (!Actor) return;
    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (Sub)
    {
        // Use the getters added to ABoidActor to locate the correct sim slot
        int32 ActorFlockIdx = Actor->GetFlockIndex();
        int32 ActorBoidIdx  = Actor->GetBoidIndex();
        if (ActorFlockIdx == FlockIndex && ActorBoidIdx != INDEX_NONE)
            Sub->DeactivateBoid(ActorFlockIdx, ActorBoidIdx);
    }
    SpawnedBoids.Remove(Actor);
}
