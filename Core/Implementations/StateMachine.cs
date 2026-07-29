using System;
using System.Collections.Generic;
using System.Linq;

namespace PalaceOfFantasy.FlockForge.Core
{
    public class StateMachine : IStateMachine
    {
        public IState CurrentState { get; private set; }

        private readonly List<IState> _states = new();
        private readonly List<IStateTransition> _transitions = new();

        public IReadOnlyList<IState> States => _states;

        public event Action<IState, IState> OnStateChanged;

        public void AddState(IState state)
        {
            if (!_states.Contains(state))
                _states.Add(state);
        }

        public void AddTransition(IStateTransition transition)
        {
            _transitions.Add(transition);
        }

        public void ForceState(IState state)
        {
            if (CurrentState == state) return;

            var oldState = CurrentState;
            CurrentState = state;
            OnStateChanged?.Invoke(oldState, state);
        }

        public void Update(IBoid boid, IBoidContext context)
        {
            var validTransitions = _transitions
                .Where(t => t.FromState == null || t.FromState == CurrentState)
                .OrderByDescending(t => t.Priority);

            foreach (var transition in validTransitions)
            {
                if (transition.Evaluate(boid, context))
                {
                    TransitionTo(boid, transition.ToState);
                    break;
                }
            }

            CurrentState?.OnUpdate(boid, context, context.DeltaTime);
        }

        private void TransitionTo(IBoid boid, IState newState)
        {
            if (CurrentState == newState) return;

            CurrentState?.OnExit(boid);
            var oldState = CurrentState;
            CurrentState = newState;
            CurrentState?.OnEnter(boid);
            OnStateChanged?.Invoke(oldState, newState);
        }
    }
}
