#pragma once

#include "CoreMinimal.h"
#include "Flock/FlockTypes.h"
#include "Behaviours/BehaviourTypes.h"

// ─────────────────────────────────────────────────────────────────────────────
//  BehaviourSystem.h
//
//  Replaces: all `class XxxBehaviour : IBehaviour` objects.
//  Every behaviour is a pure free function — no virtual dispatch, no heap.
//  The main entry point is CalculateBehaviourForce(), which dispatches
//  via a switch — branch-predictor-friendly and fully inlineable.
// ─────────────────────────────────────────────────────────────────────────────

// ── Main dispatch ─────────────────────────────────────────────────────────────
FVector CalculateBehaviourForce(
    const FBehaviourParams& Params,
    FBoidData&              Self,      // non-const: Wander mutates WanderAngle/RandSeed
    const FBoidSettings&    Settings,
    const FBoidContext&     Ctx);

// ── Individual behaviour functions (exposed for testing / direct use) ─────────
FVector Behaviour_Alignment(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx);

FVector Behaviour_Cohesion(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx);

FVector Behaviour_Separation(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float SeparationRadius);

FVector Behaviour_Seek(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx);

FVector Behaviour_Arrival(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float SlowingRadius);

FVector Behaviour_Flee(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float PanicDistance);

FVector Behaviour_Wander(
    FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float Jitter, float Radius, float Distance);

FVector Behaviour_Pursuit(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float MaxLookAheadTime);

FVector Behaviour_StayInRadius(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float BoundaryRadius);

FVector Behaviour_ObstacleAvoidance(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float AvoidanceRadius, float LookAheadDistance);

FVector Behaviour_Formation(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float ArrivalRadius);

FVector Behaviour_Command(
    const FBoidData& Self, const FBoidSettings& S, const FBoidContext& Ctx,
    float ArrivalRadius);
