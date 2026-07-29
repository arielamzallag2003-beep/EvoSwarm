using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Runtime.Behaviors
{
    /// <summary>
    /// Behaviour that overrides normal flocking to move towards a specific command target.
    /// </summary>
    public class CommandBehaviour : IBehaviour
    {
        public string Name => "Command";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority => 10;

        private FVector3? _targetPosition;
        private float _arrivalRadius;

        public CommandBehaviour(float weight, float arrivalRadius = 1.0f)
        {
            Weight = weight;
            _arrivalRadius = arrivalRadius;
        }

        public void SetTarget(FVector3? target) => _targetPosition = target;
        public bool HasTarget => _targetPosition.HasValue;

        public FVector3 CalculateForce(IBoidContext context)
        {
            if (!IsEnabled || !_targetPosition.HasValue) return FVector3.Zero;

            IBoid boid = context.Self;
            FVector3 toTarget = _targetPosition.Value - boid.Position;
            float dist = toTarget.Magnitude;

            if (dist < _arrivalRadius)
            {
                // Arrived at target
                _targetPosition = null;
                return FVector3.Zero;
            }

            // Seek force towards target
            FVector3 desiredVelocity = toTarget.Normalized * boid.Settings.MaxSpeed;
            FVector3 steering = (desiredVelocity - boid.Velocity);
            
            return steering.Normalized * Weight;
        }
    }
}
