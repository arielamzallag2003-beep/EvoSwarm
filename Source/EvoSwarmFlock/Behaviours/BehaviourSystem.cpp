#include "Behaviours/BehaviourSystem.h"
#include "Math/UnrealMathUtility.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: fast deterministic xorshift RNG stored per-boid in FBoidData
// ─────────────────────────────────────────────────────────────────────────────
static FORCEINLINE float XorshiftFloat(uint32& Seed)
{
    Seed ^= Seed << 13;
    Seed ^= Seed >> 17;
    Seed ^= Seed << 5;
    // Map [0, UINT32_MAX] → [-1, 1]
    return (static_cast<float>(Seed) / static_cast<float>(0x80000000u)) - 1.f;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Alignment
//  Steer towards the average velocity of neighbours.
//  C# source: AlignmentBehaviour.CalculateForce
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Alignment(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx)
{
    if (!Ctx.NeighborIdx || Ctx.NeighborIdx->Num() == 0) return FVector::ZeroVector;

    FVector AvgVel = FVector::ZeroVector;
    for (int32 Idx : *Ctx.NeighborIdx)
        AvgVel += (*Ctx.AllBoids)[Idx].Velocity;

    AvgVel /= static_cast<float>(Ctx.NeighborIdx->Num());

    FVector Desired = AvgVel.GetSafeNormal() * S.MaxSpeed;
    return Desired - Self.Velocity;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cohesion
//  Steer towards the average position (centre of mass) of neighbours.
//  C# source: CohesionBehaviour.CalculateForce
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Cohesion(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx)
{
    if (!Ctx.NeighborIdx || Ctx.NeighborIdx->Num() == 0) return FVector::ZeroVector;

    FVector Center = FVector::ZeroVector;
    for (int32 Idx : *Ctx.NeighborIdx)
        Center += (*Ctx.AllBoids)[Idx].Position;

    Center /= static_cast<float>(Ctx.NeighborIdx->Num());

    FVector Desired = (Center - Self.Position).GetSafeNormal() * S.MaxSpeed;
    return Desired - Self.Velocity;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Separation
//  Steer away from neighbours that are too close.
//  C# source: SeparationBehaviour.CalculateForce  (Radius = Param0)
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Separation(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float SeparationRadius)
{
    if (!Ctx.NeighborIdx || Ctx.NeighborIdx->Num() == 0) return FVector::ZeroVector;

    FVector Force = FVector::ZeroVector;
    for (int32 Idx : *Ctx.NeighborIdx)
    {
        FVector Diff = Self.Position - (*Ctx.AllBoids)[Idx].Position;
        float   Dist = Diff.Length();
        if (Dist < SeparationRadius && Dist > 1e-5f)
            Force += Diff.GetSafeNormal() / Dist;
    }
    FVector Desired = Force.GetSafeNormal() * S.MaxSpeed;
    return Desired - Self.Velocity;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Seek
//  Steer directly towards SeekTarget.
//  C# source: SeekBehaviour.CalculateForce
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Seek(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx)
{
    if (!Self.bHasSeekTarget) return FVector::ZeroVector;
    FVector Desired = (Self.SeekTarget - Self.Position).GetSafeNormal() * S.MaxSpeed;
    return Desired - Self.Velocity;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Arrival
//  Like Seek, but slow down inside the slowing radius.
//  C# source: ArrivalBehaviour.CalculateForce  (SlowingRadius = Param0)
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Arrival(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float SlowingRadius)
{
    if (!Self.bHasSeekTarget) return FVector::ZeroVector;

    FVector Offset   = Self.SeekTarget - Self.Position;
    float   Distance = Offset.Length();
    float   Ramped   = S.MaxSpeed * (Distance / FMath::Max(SlowingRadius, 0.001f));
    float   Clipped  = FMath::Min(Ramped, S.MaxSpeed);

    FVector Desired = Distance > 0.f ? (Offset / Distance) * Clipped : FVector::ZeroVector;
    return Desired - Self.Velocity;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Flee
//  Steer away from all threats within PanicDistance.
//  C# source: FleeBehaviour.CalculateForce  (PanicDistance = Param0)
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Flee(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float PanicDistance)
{
    if (!Ctx.Threats || Ctx.Threats->Num() == 0) return FVector::ZeroVector;

    FVector Force = FVector::ZeroVector;
    for (const FThreatData& T : *Ctx.Threats)
    {
        FVector Diff = Self.Position - T.Position;
        float   Dist = Diff.Length();
        if (Dist < PanicDistance)
            Force += Diff.GetSafeNormal() * (PanicDistance - Dist);
    }
    return Force.GetSafeNormal() * S.MaxSpeed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Wander
//  Smooth random steering using a projected circle on the velocity forward.
//  C# source: WanderBehaviour.CalculateForce
//  Mutable state (WanderAngle, RandSeed) lives in FBoidData — not in the
//  behaviour object.
//  Param0 = Jitter, Param1 = Radius, Param2 = Distance
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Wander(
    FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float Jitter, float Radius, float Distance)
{
    // Jitter the wander angle using our per-boid deterministic RNG
    Self.WanderAngle += XorshiftFloat(Self.RandSeed) * Jitter * Ctx.DeltaTime;

    // Wander circle point in local XZ space
    FVector CirclePoint(FMath::Cos(Self.WanderAngle) * Radius,
                        0.f,
                        FMath::Sin(Self.WanderAngle) * Radius);

    // Project circle in front of the boid
    FVector Forward   = Self.Velocity.SizeSquared() > 0.001f
                            ? Self.Velocity.GetSafeNormal()
                            : Self.Forward;
    FVector Target    = Forward * Distance + CirclePoint;

    FVector Desired   = Target.GetSafeNormal() * S.MaxSpeed;
    return Desired - Self.Velocity;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pursuit
//  Predict the future position of the seek target boid and seek there.
//  C# source: PursuitBehaviour.CalculateForce  (MaxLookAheadTime = Param0)
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Pursuit(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float MaxLookAheadTime)
{
    if (!Self.bHasSeekTarget) return FVector::ZeroVector;

    FVector Predicted = Self.SeekTarget;

    if (Ctx.AllBoids && Self.TargetBoidIndex != INDEX_NONE
        && Ctx.AllBoids->IsValidIndex(Self.TargetBoidIndex))
    {
        const FBoidData& Target = (*Ctx.AllBoids)[Self.TargetBoidIndex];
        float Distance   = FVector::Dist(Self.Position, Self.SeekTarget);
        float Speed      = Self.Velocity.Size();
        float LookAhead  = Distance / FMath::Max(Speed, S.MaxSpeed * 0.01f);
        LookAhead        = FMath::Min(LookAhead, FMath::Max(MaxLookAheadTime, 0.01f));
        Predicted       += Target.Velocity * LookAhead;
    }

    FVector Desired = (Predicted - Self.Position).GetSafeNormal() * S.MaxSpeed;
    return Desired - Self.Velocity;
}

// ─────────────────────────────────────────────────────────────────────────────
//  StayInRadius
//  Return towards anchor if boid is outside the boundary radius.
//  C# source: StayInRadiusBehaviour.CalculateForce  (BoundaryRadius = Param0)
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_StayInRadius(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float BoundaryRadius)
{
    FVector Offset   = Ctx.AnchorPosition - Self.Position;
    float   Distance = Offset.Length();

    if (Distance > BoundaryRadius)
    {
        float T = Distance / FMath::Max(BoundaryRadius, 0.001f);
        return Offset.GetSafeNormal() * (T * T);
    }
    return FVector::ZeroVector;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ObstacleAvoidance
//  Steer away from nearby obstacles; look-ahead feeler for path obstacles.
//  C# source: ObstacleAvoidanceBehaviour.CalculateForce
//  Param0 = AvoidanceRadius, Param1 = LookAheadDistance
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_ObstacleAvoidance(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float AvoidanceRadius, float LookAheadDistance)
{
    if (!Ctx.Obstacles || Ctx.Obstacles->Num() == 0) return FVector::ZeroVector;

    FVector VelNormal = Self.Velocity.SizeSquared() > 0.001f
                            ? Self.Velocity.GetSafeNormal()
                            : Self.Forward;

    FVector Force = FVector::ZeroVector;
    int32   Count = 0;

    for (const FObstacleData& Obs : *Ctx.Obstacles)
    {
        if (!Obs.bIsActive) continue;

        FVector ToObs = Obs.Position - Self.Position;
        float   Dist  = ToObs.Length();

        if (Dist < AvoidanceRadius + Obs.Radius)
        {
            // Within direct avoidance radius — push away
            FVector Away = Self.Position - Obs.Position;
            Force += Away.GetSafeNormal() / (Dist + 0.1f);
            ++Count;
        }
        else
        {
            // Feeler / look-ahead check
            float DotFwd = FVector::DotProduct(VelNormal, ToObs.GetSafeNormal());
            if (DotFwd > 0.7f && Dist < LookAheadDistance + Obs.Radius)
            {
                float Proj       = FVector::DotProduct(ToObs, VelNormal);
                FVector ProjPos  = Self.Position + VelNormal * Proj;
                float DistPath   = FVector::Dist(ProjPos, Obs.Position);

                if (DistPath < Obs.Radius + AvoidanceRadius)
                {
                    FVector AvoidDir = (Self.Position - Obs.Position).GetSafeNormal();
                    Force += AvoidDir * (LookAheadDistance / FMath::Max(Dist, 0.001f));
                    ++Count;
                }
            }
        }
    }

    return Count > 0 ? Force.GetSafeNormal() : FVector::ZeroVector;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Formation
//  Steer boid towards its pre-computed formation slot position.
//  C# source: FormationBehaviour.CalculateForce  (ArrivalRadius = Param0)
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Formation(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float ArrivalRadius)
{
    if (!Self.bInFormation) return FVector::ZeroVector;

    FVector ToSlot = Self.FormationSlot - Self.Position;
    float   Dist   = ToSlot.Length();

    if (Dist < ArrivalRadius)
    {
        // Smoothly decelerate inside arrival radius
        float T = (ArrivalRadius > 0.f) ? (Dist / ArrivalRadius) : 0.f;
        return (ToSlot.GetSafeNormal() * S.MaxSpeed * T - Self.Velocity);
    }
    return ToSlot.GetSafeNormal() * S.MaxSpeed - Self.Velocity;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Command
//  Override normal flocking; seek a specific command-issued target position.
//  C# source: CommandBehaviour.CalculateForce  (ArrivalRadius = Param0)
// ─────────────────────────────────────────────────────────────────────────────
FVector Behaviour_Command(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float ArrivalRadius)
{
    if (!Self.bHasCommandTarget) return FVector::ZeroVector;

    FVector ToTarget = Self.CommandTarget - Self.Position;
    float   Dist     = ToTarget.Length();

    if (Dist < ArrivalRadius) return FVector::ZeroVector; // arrived

    FVector DesiredVel = ToTarget.GetSafeNormal() * S.MaxSpeed;
    return (DesiredVel - Self.Velocity).GetSafeNormal(); // normalised steering
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main dispatch
// ─────────────────────────────────────────────────────────────────────────────
FVector CalculateBehaviourForce(
    const FBehaviourParams& P,
    FBoidData&              Self,
    const FBoidSettings&    S,
    const FBoidContext&     Ctx)
{
    switch (P.Type)
    {
    case EBehaviourType::Alignment:
        return Behaviour_Alignment(Self, S, Ctx);

    case EBehaviourType::Cohesion:
        return Behaviour_Cohesion(Self, S, Ctx);

    case EBehaviourType::Separation:
        return Behaviour_Separation(Self, S, Ctx, P.Param0 > 0.f ? P.Param0 : 2.f);

    case EBehaviourType::Seek:
        return Behaviour_Seek(Self, S, Ctx);

    case EBehaviourType::Arrival:
        return Behaviour_Arrival(Self, S, Ctx, P.Param0 > 0.f ? P.Param0 : 5.f);

    case EBehaviourType::Flee:
        return Behaviour_Flee(Self, S, Ctx, P.Param0 > 0.f ? P.Param0 : 5.f);

    case EBehaviourType::Wander:
        return Behaviour_Wander(Self, S, Ctx,
            P.Param0 > 0.f ? P.Param0 : 5.f,   // Jitter
            P.Param1 > 0.f ? P.Param1 : 2.f,   // Radius
            P.Param2 > 0.f ? P.Param2 : 3.f);  // Distance

    case EBehaviourType::Pursuit:
        return Behaviour_Pursuit(Self, S, Ctx, P.Param0 > 0.f ? P.Param0 : 2.f);

    case EBehaviourType::StayInRadius:
        return Behaviour_StayInRadius(Self, S, Ctx, P.Param0 > 0.f ? P.Param0 : 20.f);

    case EBehaviourType::ObstacleAvoidance:
        return Behaviour_ObstacleAvoidance(Self, S, Ctx,
            P.Param0 > 0.f ? P.Param0 : 2.f,   // AvoidanceRadius
            P.Param1 > 0.f ? P.Param1 : 5.f);  // LookAheadDistance

    case EBehaviourType::Formation:
        return Behaviour_Formation(Self, S, Ctx, P.Param0 > 0.f ? P.Param0 : 1.f);

    case EBehaviourType::Command:
        return Behaviour_Command(Self, S, Ctx, P.Param0 > 0.f ? P.Param0 : 1.f);

    default:
        return FVector::ZeroVector;
    }
}
