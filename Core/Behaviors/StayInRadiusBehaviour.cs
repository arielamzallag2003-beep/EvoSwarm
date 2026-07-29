using System;

namespace PalaceOfFantasy.FlockForge.Core.Behaviors
{
    public class StayInRadiusBehaviour : IBehaviour
    {
        public string Name => "StayInRadius";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority => 0;

        public float Radius { get; set; } = 20f;

        public FVector3 CalculateForce(IBoidContext context)
        {
            var center = context.Flock.Settings.AnchorPosition;
            var offset = center - context.Self.Position;
            var distance = offset.Magnitude;

            if (distance > Radius)
            {
                // Ramp up force as we get further away
                 var t = distance / Radius;
                 return offset.Normalized * (t * t);
            }
            return FVector3.Zero;
        }
    }
}
