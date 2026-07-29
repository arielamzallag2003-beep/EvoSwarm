using System;
using System.Collections.Generic;
using System.Linq;

namespace PalaceOfFantasy.FlockForge.Core
{
    public class FlockStateMachineBehaviour : IBehaviour
    {
        private readonly Func<IStateMachine> _factory;
        private readonly Dictionary<int, IStateMachine> _machines = new();

        public IStateMachine GetMachine(int boidId) => _machines.TryGetValue(boidId, out var machine) ? machine : null;
        
        public string Name { get; } = "StateMachine";
        public float Weight { get; set; }
        public bool IsEnabled { get; set; }
        public int Priority => 0;

        public FlockStateMachineBehaviour(Func<IStateMachine> factory, float weight = 1f, bool isEnabled = true)
        {
            _factory = factory;
            Weight = weight;
            IsEnabled = isEnabled;
        }

        public FVector3 CalculateForce(IBoidContext context)
        {
            if (!IsEnabled) return FVector3.Zero;

            if (!_machines.TryGetValue(context.Self.Id, out var machine))
            {
                machine = _factory();
                _machines[context.Self.Id] = machine;
                if (context.Self.Id == 0) UnityEngine.Debug.Log($"[Boid 0] Created state machine, CurrentState = {machine.CurrentState?.Name ?? "NULL"}");
            }

            machine.Update(context.Self, context);

            if (machine.CurrentState == null)
            {
                if (context.Self.Id == 0) UnityEngine.Debug.LogWarning($"[Boid 0] No Current State!");
                return FVector3.Zero;
            }

            FVector3 totalForce = FVector3.Zero;
            float remainingAuthority = 1.0f;

            var sortedBehaviours = machine.CurrentState.Behaviours
                .Where(b => b.IsEnabled)
                .OrderByDescending(b => b.Priority)
                .ToList();

            foreach (var behaviour in sortedBehaviours)
            {
                if (remainingAuthority <= 0.01f) break;

                var f = behaviour.CalculateForce(context) * behaviour.Weight;
                float forceMagnitude = f.Magnitude;

                totalForce += f * remainingAuthority;

                if (behaviour.Priority > 0 && forceMagnitude > 0.1f)
                {
                    float dampening = (behaviour.Priority / 20.0f) * (forceMagnitude / context.Settings.MaxSpeed);
                    remainingAuthority = Math.Max(0, remainingAuthority - dampening);
                }
            }
            
            if (context.Self.Id == 0 && totalForce.SqrMagnitude < 0.001f)
            {
                UnityEngine.Debug.LogWarning($"[Boid 0] State '{machine.CurrentState.Name}' has {machine.CurrentState.Behaviours.Count} behaviours but total force is ZERO");
            }
            
            return totalForce;
        }
    }
}
