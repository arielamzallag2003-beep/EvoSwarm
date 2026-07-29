using System;
using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IStateMachine
    {
        IState CurrentState { get; }
        IReadOnlyList<IState> States { get; }

        void AddState(IState state);
        void AddTransition(IStateTransition transition);
        void ForceState(IState state);
        void Update(IBoid boid, IBoidContext context);

        event Action<IState, IState> OnStateChanged;
    }

    public interface IStateTransition
    {
        IState FromState { get; }
        IState ToState { get; }
        int Priority { get; }
        bool Evaluate(IBoid boid, IBoidContext context);
    }
}
