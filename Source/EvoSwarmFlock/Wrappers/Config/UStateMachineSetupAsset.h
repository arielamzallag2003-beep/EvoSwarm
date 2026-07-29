#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StateMachine/StateMachineTypes.h"
#include "Flock/FlockContainer.h"
#include "UStateMachineSetupAsset.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  UStateMachineSetupAsset
//
//  Replaces: StateMachineAsset.cs  (ScriptableObject — builds IStateMachine)
//            StateAsset.cs         (ScriptableObject — holds IState data)
//            TransitionAsset.cs    (ScriptableObject — IStateTransition.Evaluate)
//
//  All three Unity assets have been collapsed into one DataAsset:
//    States      — array of FStateDefinition (name + behaviour stack)
//    Transitions — array of FTransitionRule  (from/to index + condition)
//
//  Call ApplyToFlock() once on BeginPlay (or whenever configuration changes).
//  The asset does NOT hold OnEnter/OnExit TFunctions — those are code-only
//  and must be assigned in C++ after ApplyToFlock if needed.
// ─────────────────────────────────────────────────────────────────────────────
UCLASS(BlueprintType)
class EVOSWARMFLOCK_API UStateMachineSetupAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    /**
     * State definitions — each entry holds a name and a behaviour stack.
     * Index 0 = initial/default state (mirrors StateMachineAsset._initialState).
     * FStateDefinition::bBehavioursDirty is set true by default so the
     * subsystem will sort on first use.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "States")
    TArray<FStateDefinition> States;

    /**
     * Transition rules evaluated each tick per boid.
     * Use FromState = 255 for "from any state" (mirrors null FromState in C#).
     * Sorted descending by Priority at runtime.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transitions")
    TArray<FTransitionRule> Transitions;

    /**
     * Push this setup into the flock.
     * Overwrites FFlock::States and FFlock::Transitions and marks them dirty
     * so the subsystem re-sorts on the next tick.
     */
    UFUNCTION(BlueprintCallable, Category = "EvoSwarm")
    void ApplyToFlock(UPARAM(ref) FFlock& Flock) const
    {
        Flock.States      = States;
        Flock.Transitions = Transitions;

        // Mark all state behaviour stacks dirty so they get re-sorted
        for (FStateDefinition& S : Flock.States)
            S.bBehavioursDirty = true;

        Flock.bTransitionsDirty = true;
    }
};
