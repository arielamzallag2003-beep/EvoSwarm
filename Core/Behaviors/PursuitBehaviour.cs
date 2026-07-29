using System;

namespace PalaceOfFantasy.FlockForge.Core.Behaviors
{
    /// <summary>
    /// Predicts the future position of a moving target and steers towards it.
    /// Extension of Seek that accounts for target velocity.
    /// </summary>
    public class PursuitBehaviour : IBehaviour
    {
        public string Name => "Pursuit";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority { get; set; } = 0;

        public FVector3 CalculateForce(IBoidContext context)
        {
            var targetBoid = context.SeekTargetBoid;
            var targetPos = context.SeekTarget;

            if (!targetPos.HasValue) return FVector3.Zero;

            FVector3 predictedPosition = targetPos.Value;

            if (targetBoid != null)
            {
                float distance = FVector3.Distance(context.Self.Position, targetPos.Value);
                
                // Look-ahead time is proportional to distance and inversely proportional to current speed
                float speed = context.Self.Velocity.Magnitude;
                float lookAheadTime = distance / (speed > 0.1f ? speed : context.Settings.MaxSpeed);
                
                // Clamp look-ahead to avoid extreme predictions
                lookAheadTime = Math.Min(lookAheadTime, 2.0f);

                predictedPosition += targetBoid.Velocity * lookAheadTime;
            }

            FVector3 desired = (predictedPosition - context.Self.Position).Normalized * context.Settings.MaxSpeed;
            return desired - context.Self.Velocity;
        }
    }
}
