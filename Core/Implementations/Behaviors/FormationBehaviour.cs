using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Runtime.Behaviors
{
    public class FormationBehaviour : IBehaviour
    {
        public string Name => "Formation";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority => 5;

        public IFormationController Controller { get; set; }
        private float _arrivalRadius;

        public FormationBehaviour(float weight, IFormationController controller, float arrivalRadius = 1.0f)
        {
            Weight = weight;
            Controller = controller;
            _arrivalRadius = arrivalRadius;
        }

        public FVector3 CalculateForce(IBoidContext context)
        {
            IBoid boid = context.Self;

            if (Controller == null && boid.Flock != null)
            {
                // Dynamic lookup logic if needed
            }

            if (Controller == null || !Controller.IsActive || Controller.CurrentFormation == null) return FVector3.Zero;

            FVector3? targetPos = Controller.GetTargetPosition(boid);
            if (!targetPos.HasValue) return FVector3.Zero;

            FVector3 toTarget = targetPos.Value - boid.Position;
            float dist = toTarget.Magnitude;

            if (dist < _arrivalRadius)
            {
                // Slow down as we arrive
                return (toTarget.Normalized * boid.Settings.MaxSpeed * (dist / _arrivalRadius) - boid.Velocity) * Weight;
            }

            // Seek force towards slot
            return (toTarget.Normalized * boid.Settings.MaxSpeed - boid.Velocity) * Weight;
        }
    }
}
