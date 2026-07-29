using System;

namespace PalaceOfFantasy.FlockForge.Core.Behaviors
{
    public class ArrivalBehaviour : IBehaviour
    {
        public string Name => "Arrival";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority => 0;
        public float SlowingRadius { get; set; } = 5f;

        public FVector3 CalculateForce(IBoidContext context)
        {
            if (context.SeekTarget == null) return FVector3.Zero;
            
            var targetOffset = context.SeekTarget.Value - context.Self.Position;
            var distance = targetOffset.Magnitude;
            var rampedSpeed = context.Settings.MaxSpeed * (distance / SlowingRadius);
            var clippedSpeed = (float)Math.Min(rampedSpeed, context.Settings.MaxSpeed);
            
            var desired = (distance > 0) ? (targetOffset / distance) * clippedSpeed : FVector3.Zero;
            
            return desired - context.Self.Velocity;
        }
    }
}
