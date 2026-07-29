using System;
using System.Collections.Generic;
using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Config.AI
{
    [CreateAssetMenu(menuName = "FlockForge/AI/State")]
    public class StateAsset : ScriptableObject
    {
        [SerializeField] private List<BehaviourAsset> _behaviours = new();
        [SerializeField] private List<StateTransitionConfig> _transitions = new();

        public IReadOnlyList<BehaviourAsset> Behaviours => _behaviours;
        public IReadOnlyList<StateTransitionConfig> Transitions => _transitions;

        public IState CreateState()
        {
            var runtimeBehaviours = new List<IBehaviour>();
            foreach (var asset in _behaviours)
            {
                if (asset != null)
                    runtimeBehaviours.Add(asset.CreateBehaviour());
            }
            return new State(name, runtimeBehaviours);
        }
    }

    [Serializable]
    public struct StateTransitionConfig
    {
        public TransitionAsset Condition;
        public StateAsset TargetState;
        public int Priority;
    }
}
