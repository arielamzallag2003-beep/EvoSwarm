using System;
using System.Collections.Generic;
using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Config.AI
{
    [CreateAssetMenu(menuName = "FlockForge/AI/State Machine")]
    public class StateMachineAsset : BehaviourAsset
    {
        [SerializeField] private StateAsset _initialState;
        [SerializeField] private List<StateAsset> _allStates = new();

        public override IBehaviour CreateBehaviour()
        {
            return new FlockStateMachineBehaviour(CreateRuntimeStateMachine, _weight, _isEnabled);
        }

        private IStateMachine CreateRuntimeStateMachine()
        {
            var machine = new StateMachine();
            var stateMap = new Dictionary<StateAsset, IState>();

            // 1. Create all states
            foreach (var stateAsset in _allStates)
            {
                if (stateAsset == null) continue;
                var state = stateAsset.CreateState();
                stateMap[stateAsset] = state;
                machine.AddState(state);
            }

            // Ensure initial state is in the list
            if (_initialState != null && !stateMap.ContainsKey(_initialState))
            {
                var state = _initialState.CreateState();
                stateMap[_initialState] = state;
                machine.AddState(state);
            }

            // 2. Create transitions
            foreach (var stateAsset in stateMap.Keys)
            {
                var fromState = stateMap[stateAsset];

                foreach (var transitionConfig in stateAsset.Transitions)
                {
                    if (transitionConfig.TargetState == null || transitionConfig.Condition == null) continue;

                    if (stateMap.TryGetValue(transitionConfig.TargetState, out var toState))
                    {
                        var transition = new ConfigurableTransition(
                            fromState,
                            toState,
                            transitionConfig.Priority,
                            transitionConfig.Condition
                        );
                        machine.AddTransition(transition);
                    }
                }
            }

            // 3. Set initial state
            if (_initialState != null && stateMap.ContainsKey(_initialState))
            {
                machine.ForceState(stateMap[_initialState]);
            }

            return machine;
        }

        private class ConfigurableTransition : IStateTransition
        {
            public IState FromState { get; }
            public IState ToState { get; }
            public int Priority { get; }

            private readonly TransitionAsset _condition;

            public ConfigurableTransition(IState from, IState to, int priority, TransitionAsset condition)
            {
                FromState = from;
                ToState = to;
                Priority = priority;
                _condition = condition;
            }

            public bool Evaluate(IBoid boid, IBoidContext context)
            {
                return _condition.Evaluate(boid, context);
            }
        }
    }
}
