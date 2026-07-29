#include "StateMachine/StateMachineSystem.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Condition evaluation
// ─────────────────────────────────────────────────────────────────────────────
bool EvaluateTransitionCondition(
    ETransitionCondition Condition,
    float                Threshold,
    const FBoidData&     Boid,
    const FBoidContext&  Ctx)
{
    switch (Condition)
    {
    case ETransitionCondition::Always:
        return true;

    case ETransitionCondition::HasSeekTarget:
        return Boid.bHasSeekTarget;

    case ETransitionCondition::NoSeekTarget:
        return !Boid.bHasSeekTarget;

    case ETransitionCondition::HasThreats:
        return Ctx.Threats && Ctx.Threats->Num() > 0;

    case ETransitionCondition::NoThreats:
        return !Ctx.Threats || Ctx.Threats->Num() == 0;

    case ETransitionCondition::SpeedAboveThreshold:
        return Boid.Velocity.SizeSquared() > Threshold * Threshold;

    case ETransitionCondition::SpeedBelowThreshold:
        return Boid.Velocity.SizeSquared() < Threshold * Threshold;

    case ETransitionCondition::NearTarget:
        if (!Boid.bHasSeekTarget) return false;
        return FVector::DistSquared(Boid.Position, Boid.SeekTarget) < Threshold * Threshold;

    case ETransitionCondition::FarFromTarget:
        if (!Boid.bHasSeekTarget) return false;
        return FVector::DistSquared(Boid.Position, Boid.SeekTarget) > Threshold * Threshold;

    case ETransitionCondition::InFormation:
        return Boid.bInFormation;

    case ETransitionCondition::NotInFormation:
        return !Boid.bInFormation;

    case ETransitionCondition::HasCommandTarget:
        return Boid.bHasCommandTarget;

    case ETransitionCondition::NoCommandTarget:
        return !Boid.bHasCommandTarget;

    default:
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  UpdateStateMachine
//  Replaces: StateMachine.Update + FlockStateMachineBehaviour.CalculateForce
//            (the state-management part only — force calculation is separate)
// ─────────────────────────────────────────────────────────────────────────────
void UpdateStateMachine(
    FBoidData&                      Boid,
    TArray<FStateDefinition>&       States,
    const TArray<FTransitionRule>&  Transitions,
    const FBoidContext&             Ctx,
    int32&                          OutPrevState)
{
    OutPrevState = INDEX_NONE;

    // Iterate over pre-sorted transitions (highest Priority first)
    for (const FTransitionRule& Rule : Transitions)
    {
        // 255 = "from any state" (mirrors C# IStateTransition.FromState == null)
        if (Rule.FromState != 255 && Rule.FromState != Boid.StateIndex)
            continue;

        if (EvaluateTransitionCondition(Rule.Condition, Rule.Threshold, Boid, Ctx))
        {
            if (Boid.StateIndex == Rule.ToState) break; // already in target state

            // ── OnExit: call on the state we are LEAVING ──────────────────
            if (States.IsValidIndex(Boid.StateIndex))
            {
                FStateDefinition& OldState = States[Boid.StateIndex];
                if (OldState.OnExit)
                    OldState.OnExit(Boid);
            }

            // ── Advance state index ────────────────────────────────────────
            OutPrevState    = Boid.StateIndex;
            Boid.PrevState  = Boid.StateIndex;
            Boid.StateIndex = Rule.ToState;

            // ── OnEnter: call on the state we are ENTERING ────────────────
            // Context is passed so entry code can read neighbours, threats, etc.
            if (States.IsValidIndex(Boid.StateIndex))
            {
                FStateDefinition& NewState = States[Boid.StateIndex];
                if (NewState.OnEnter)
                    NewState.OnEnter(Boid, Ctx);
            }

            break; // only the highest-priority matching rule fires per tick
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SortStateMachineData
// ─────────────────────────────────────────────────────────────────────────────
void SortStateMachineData(
    TArray<FStateDefinition>& States,
    TArray<FTransitionRule>&  Transitions)
{
    // Sort transitions descending by Priority
    Transitions.Sort([](const FTransitionRule& A, const FTransitionRule& B)
    {
        return A.Priority > B.Priority;
    });

    // Sort each state's behaviour stack descending by Priority
    for (FStateDefinition& State : States)
    {
        if (State.bBehavioursDirty)
        {
            State.Behaviours.Sort([](const FBehaviourEntry& A, const FBehaviourEntry& B)
            {
                return A.Params.Priority > B.Params.Priority;
            });
            State.bBehavioursDirty = false;
        }
    }
}
