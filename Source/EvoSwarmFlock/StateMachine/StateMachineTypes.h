#pragma once

#include "CoreMinimal.h"
#include "Behaviours/BehaviourTypes.h"
#include "Flock/FlockTypes.h"       // FBoidData, FBoidContext forward types
#include "StateMachineTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  StateMachineTypes.h
//
//  Replaces:
//    - IStateMachine / StateMachine (heap object, Dictionary<int, IStateMachine>)
//    - IState / State (heap objects with virtual OnEnter/OnExit/OnUpdate)
//    - IStateTransition (virtual Evaluate)
//    - FlockStateMachineBehaviour (per-boid dictionary)
//
//  DOD approach:
//    - FBoidData::StateIndex  (uint8) is the entire per-boid state machine
//    - FStateDefinition       stores the behaviour stack + OnEnter/OnExit hooks
//    - OnUpdate is implicit: the behaviour stack runs every tick
//    - FTransitionRule        is a POD condition evaluated per boid per tick
//    - FFlock::States / Transitions are shared across all boids in the flock
// ─────────────────────────────────────────────────────────────────────────────

// ── Transition condition types ───────────────────────────────────────────────
UENUM(BlueprintType)
enum class ETransitionCondition : uint8
{
    Always               UMETA(DisplayName = "Always"),
    HasSeekTarget        UMETA(DisplayName = "Has Seek Target"),
    NoSeekTarget         UMETA(DisplayName = "No Seek Target"),
    HasThreats           UMETA(DisplayName = "Has Threats"),
    NoThreats            UMETA(DisplayName = "No Threats"),
    SpeedAboveThreshold  UMETA(DisplayName = "Speed Above Threshold"),
    SpeedBelowThreshold  UMETA(DisplayName = "Speed Below Threshold"),
    NearTarget           UMETA(DisplayName = "Near Target (dist < Threshold)"),
    FarFromTarget        UMETA(DisplayName = "Far From Target (dist > Threshold)"),
    InFormation          UMETA(DisplayName = "In Formation"),
    NotInFormation       UMETA(DisplayName = "Not In Formation"),
    HasCommandTarget     UMETA(DisplayName = "Has Command Target"),
    NoCommandTarget      UMETA(DisplayName = "No Command Target"),
};

// ── State definition ─────────────────────────────────────────────────────────
//
//  Replaces IState / State:
//    Name        → FString Name
//    Behaviours  → TArray<FBehaviourEntry> Behaviours  (DOD stack)
//    OnUpdate()  → implicit: behaviour stack runs every tick
//    OnEnter()   → TFunction<void(FBoidData&, const FBoidContext&)> OnEnter
//    OnExit()    → TFunction<void(FBoidData&)>                       OnExit
//
//  TFunction is not a UPROPERTY (not Blueprint-serialisable) but it lets
//  gameplay code assign any lambda or free function at runtime,
//  exactly like C# virtual overrides — minus the vtable.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FStateDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    /** Behaviour stack — MUST be sorted descending by Priority before first use. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBehaviourEntry> Behaviours;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bBehavioursDirty = true;

    // ── Lifecycle hooks (replaces IState virtual methods) ─────────────────
    //
    // OnEnter: called once when a boid enters this state.
    //   Receives the boid (mutable) and its current context.
    //   Use for: resetting per-boid state, playing entry particles, etc.
    //
    // OnExit: called once when a boid leaves this state.
    //   Receives only the boid (context may be stale at exit time).
    //   Use for: cleanup, stopping sounds, clearing command targets, etc.
    //
    // Leave unset (nullptr) for states that need no lifecycle side-effects.
    TFunction<void(FBoidData& Boid, const FBoidContext& Ctx)> OnEnter;
    TFunction<void(FBoidData& Boid)>                          OnExit;
};

// ── Transition rule ──────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FTransitionRule
{
    GENERATED_BODY()

    /** 255 = "from any state" (replaces IStateTransition.FromState == null). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 FromState = 255;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    uint8 ToState = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETransitionCondition Condition = ETransitionCondition::Always;

    /** Context-dependent threshold (speed, distance, etc.). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Threshold = 0.f;
};

/** Sort: highest Priority first — call once when transitions are configured. */
struct FTransitionPrioritySorter
{
    FORCEINLINE bool operator()(const FTransitionRule& A, const FTransitionRule& B) const
    {
        return A.Priority > B.Priority;
    }
};
