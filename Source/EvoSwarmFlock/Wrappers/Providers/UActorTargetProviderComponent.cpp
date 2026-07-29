#include "Wrappers/Providers/UActorTargetProviderComponent.h"
#include "Wrappers/Components/UFlockManagerComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/PrimitiveComponent.h"
#include "CollisionQueryParams.h"

UActorTargetProviderComponent::UActorTargetProviderComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UActorTargetProviderComponent::BeginPlay()
{
    Super::BeginPlay();
    // Auto-detect FlockIndex from sibling UFlockManagerComponent
    if (FlockIndex == INDEX_NONE)
    {
        if (UFlockManagerComponent* Mgr = GetOwner()->FindComponentByClass<UFlockManagerComponent>())
            FlockIndex = Mgr->GetFlockIndex();
    }
}

void UActorTargetProviderComponent::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Func)
{
    Super::TickComponent(DeltaTime, TickType, Func);

    UFlockSubsystem* Sub = GetWorld()->GetSubsystem<UFlockSubsystem>();
    if (!Sub) return;
    FFlock* Flock = Sub->GetFlock(FlockIndex);
    if (!Flock) return;

    UpdateSeekTargets(*Flock);
    UpdateThreats(*Flock);
    if (bRunObstacleOverlapQuery)
        UpdateObstacles(*Flock);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Seek targets — push SeekTargetActor's position into every active boid
// ─────────────────────────────────────────────────────────────────────────────
void UActorTargetProviderComponent::UpdateSeekTargets(FFlock& Flock)
{
    if (!SeekTargetActor) return;
    FVector TargetPos = SeekTargetActor->GetActorLocation();

    for (FBoidData& Boid : Flock.Boids)
    {
        if (!Boid.bIsActive) continue;
        Boid.SeekTarget     = TargetPos;
        Boid.bHasSeekTarget = true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Threats — rebuild FFlock::Threats from actor positions each tick
// ─────────────────────────────────────────────────────────────────────────────
void UActorTargetProviderComponent::UpdateThreats(FFlock& Flock)
{
    Flock.Threats.Reset(ThreatActors.Num());
    for (const TObjectPtr<AActor>& Actor : ThreatActors)
    {
        if (!Actor) continue;
        FThreatData T;
        T.Position = Actor->GetActorLocation();
        Flock.Threats.Add(T);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Obstacles — physics overlap query, mirrors TransformTargetProvider.GetNearbyObstacles
//  Uses the flock anchor as query origin with max ObstacleAvoidanceDistance as radius.
// ─────────────────────────────────────────────────────────────────────────────
void UActorTargetProviderComponent::UpdateObstacles(FFlock& Flock)
{
    if (Flock.SettingsTemplates.Num() == 0) return;

    // Find largest obstacle avoidance radius across all settings templates
    float QueryRadius = 0.f;
    for (const FBoidSettings& S : Flock.SettingsTemplates)
        QueryRadius = FMath::Max(QueryRadius, S.ObstacleAvoidanceDistance);

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(TargetProviderObstacleQuery), false);
    Params.AddIgnoredActor(GetOwner());

    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        Flock.AnchorPosition,
        FQuat::Identity,
        ObstacleQueryChannel,
        FCollisionShape::MakeSphere(QueryRadius),
        Params);

    // Append/update dynamic obstacles (we don't track slots for dynamic results —
    // dynamic obstacles are rebuilt fully each tick, unlike registered static ones)
    Flock.Obstacles.Reset(Overlaps.Num());
    for (const FOverlapResult& Hit : Overlaps)
    {
        if (!Hit.GetActor()) continue;
        FObstacleData Obs;
        Obs.Position  = Hit.GetActor()->GetActorLocation();
        Obs.Radius    = Hit.GetComponent()->Bounds.SphereRadius;
        Obs.bIsActive = Hit.GetActor()->IsHidden() == false;
        Flock.Obstacles.Add(Obs);
    }
}
