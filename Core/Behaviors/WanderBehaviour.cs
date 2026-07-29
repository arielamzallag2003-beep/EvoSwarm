using System;

namespace PalaceOfFantasy.FlockForge.Core.Behaviors
{
    public class WanderBehaviour : IBehaviour
    {
        public string Name => "Wander";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority => 0;

        public float Jitter { get; set; } = 5f;
        public float Radius { get; set; } = 2f;
        public float Distance { get; set; } = 3f;

        private Random _random = new Random();
        private FVector3 _wanderTarget;

        public WanderBehaviour()
        {
            // Initialize with a random direction
            _wanderTarget = new FVector3((float)_random.NextDouble() * 2 - 1, 0, (float)_random.NextDouble() * 2 - 1).Normalized;
        }

        public FVector3 CalculateForce(IBoidContext context)
        {
            // Add a small random vector to the target's position
            FVector3 randomJitter = new FVector3((float)_random.NextDouble() * 2 - 1, 0, (float)_random.NextDouble() * 2 - 1) * Jitter;
            _wanderTarget += randomJitter * (1.0f / 60.0f); // Frame-independent jitter approximation
            _wanderTarget = _wanderTarget.Normalized;

            // Project the target in front of the boid
            FVector3 targetPos = _wanderTarget * Radius;
            FVector3 forward = context.Self.Velocity.Magnitude > 0 ? context.Self.Velocity.Normalized : context.Self.Forward;
            FVector3 projectedCenter = forward * Distance;

            var desired = (projectedCenter + targetPos).Normalized * context.Settings.MaxSpeed;
            return desired - context.Self.Velocity;
        }
    }
}
