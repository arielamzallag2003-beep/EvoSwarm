#pragma once

#include "CoreMinimal.h"
#include "Flock/FlockTypes.h"
#include "StateMachine/StateMachineTypes.h"
#include "Behaviours/BehaviourTypes.h"

// ─────────────────────────────────────────────────────────────────────────────
//  StateMachineSystem.h
//
//  Replaces: IStateMachine / StateMachine heap objects and the
//            FlockStateMachineBehaviour Dictionary<int, IStateMachine>.
//
//  The entire per-boid state is: uint8 FBoidData::StateIndex
//  All logic lives in these free functions — no heap allocation.
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Evaluate transition rules and update Boid.StateIndex if a condition fires.
 * Calls OnExit on the old state and OnEnter on the new state.
 * Call once per active boid during the simulation tick (before force calculation).
 *
 * @param Boid        The boid to update (StateIndex may be mutated).
 * @param States      Shared state definitions (needed to call OnEnter/OnExit).
 * @param Transitions Shared transition table (sorted descending by Priority).
 * @param Ctx         Read-only context for condition evaluation and OnEnter arg.
 * @param OutPrevState Set to the old state index if a transition occurred, else INDEX_NONE.
 */
void UpdateStateMachine(
    FBoidData&                      Boid,
    TArray<FStateDefinition>&       States,
    const TArray<FTransitionRule>&  Transitions,
    const FBoidContext&             Ctx,
    int32&                          OutPrevState);

/**
 * Evaluate a single transition condition.
 * Exposed for unit testing.
 */
bool EvaluateTransitionCondition(
    ETransitionCondition Condition,
    float                Threshold,
    const FBoidData&     Boid,
    const FBoidContext&  Ctx);

/**
 * Sort the States and Transitions arrays by priority.
 * Call whenever the configuration changes (not every tick).
 */
void SortStateMachineData(
    TArray<FStateDefinition>& States,
    TArray<FTransitionRule>&  Transitions);
