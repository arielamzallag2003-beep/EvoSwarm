#pragma once

#include "CoreMinimal.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FlockEvents.h
//
//  Replaces C# EventManager (Dictionary<Type, Delegate> + boxing).
//  Events are collected into FPendingEvent during the tick loop and
//  fired via multicast delegates AFTER the tick (no delegate dispatch
//  in the hot simulation path).
// ─────────────────────────────────────────────────────────────────────────────

// ── Multicast delegate declarations ─────────────────────────────────────────
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBoidStateChanged,
    int32  /*BoidIndex*/,
    uint8  /*NewStateIndex*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBoidCaptured,
    int32  /*PreyBoidIndex*/,
    int32  /*PredatorBoidIndex*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnThreatDetected,
    int32   /*BoidIndex*/,
    FVector /*ThreatPosition*/);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTargetReached,
    int32   /*BoidIndex*/,
    FVector /*TargetPosition*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnBoidAdded,   int32 /*BoidIndex*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnBoidRemoved, int32 /*BoidIndex*/);

// ── Pending event types ──────────────────────────────────────────────────────
UENUM()
enum class EFlockEventType : uint8
{
    StateChanged,
    BoidCaptured,
    ThreatDetected,
    TargetReached,
};

// POD struct collected during the tick; dispatched post-tick.
struct FPendingFlockEvent
{
    EFlockEventType Type;
    int32  IndexA   = INDEX_NONE;   // BoidIndex / PreyIndex
    int32  IndexB   = INDEX_NONE;   // PredatorIndex / NewState
    FVector VectorA = FVector::ZeroVector; // ThreatPosition / TargetPosition
};

// ── Event hub owned by the flock subsystem ──────────────────────────────────
struct FFlockEventHub
{
    FOnBoidStateChanged OnStateChanged;
    FOnBoidCaptured     OnBoidCaptured;
    FOnThreatDetected   OnThreatDetected;
    FOnTargetReached    OnTargetReached;
    FOnBoidAdded        OnBoidAdded;
    FOnBoidRemoved      OnBoidRemoved;

    TArray<FPendingFlockEvent> PendingEvents;  // written during tick

    FORCEINLINE void Push(FPendingFlockEvent&& Evt)
    {
        PendingEvents.Add(MoveTemp(Evt));
    }

    /** Call once per frame AFTER the simulation tick. */
    void Flush()
    {
        for (const FPendingFlockEvent& E : PendingEvents)
        {
            switch (E.Type)
            {
            case EFlockEventType::StateChanged:
                OnStateChanged.Broadcast(E.IndexA, static_cast<uint8>(E.IndexB));
                break;
            case EFlockEventType::BoidCaptured:
                OnBoidCaptured.Broadcast(E.IndexA, E.IndexB);
                break;
            case EFlockEventType::ThreatDetected:
                OnThreatDetected.Broadcast(E.IndexA, E.VectorA);
                break;
            case EFlockEventType::TargetReached:
                OnTargetReached.Broadcast(E.IndexA, E.VectorA);
                break;
            }
        }
        PendingEvents.Reset();
    }
};
