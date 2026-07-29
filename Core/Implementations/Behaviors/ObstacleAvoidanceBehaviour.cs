using System.Collections.Generic;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Runtime.Behaviors
{
    public class ObstacleAvoidanceBehaviour : IBehaviour
    {
        public string Name => "ObstacleAvoidance";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority => 20;

        private float _avoidanceRadius;
        private float _lookAheadDistance;

        public ObstacleAvoidanceBehaviour(float weight, float avoidanceRadius, float lookAheadDistance)
        {
            Weight = weight;
            _avoidanceRadius = avoidanceRadius;
            _lookAheadDistance = lookAheadDistance;
        }

        public FVector3 CalculateForce(IBoidContext context)
        {
            if (!IsEnabled) return FVector3.Zero;

            IBoid boid = context.Self;
            FVector3 force = FVector3.Zero;
            int count = 0;

            // Simple raycast-like check for obstacles in front
            FVector3 velocityNormal = boid.Velocity.Normalized;
            if (velocityNormal.SqrMagnitude < 0.001f)
                velocityNormal = boid.Forward;

            foreach (var obstacle in context.NearbyObstacles)
            {
                if (!obstacle.IsActive) continue;

                FVector3 toObstacle = obstacle.Position - boid.Position;
                float dist = toObstacle.Magnitude;

                // If within avoidance radius or in path
                if (dist < _avoidanceRadius + obstacle.Radius)
                {
                    // Push away from obstacle center
                    FVector3 away = boid.Position - obstacle.Position;
                    force += away.Normalized / (dist + 0.1f);
                    count++;
                }
                else
                {
                    // Check if it's in front
                    float dot = FVector3.Dot(velocityNormal, toObstacle.Normalized);
                    if (dot > 0.7f && dist < _lookAheadDistance + obstacle.Radius)
                    {
                        // Project boid position onto velocity line
                        FVector3 projection = boid.Position + velocityNormal * FVector3.Dot(toObstacle, velocityNormal);
                        float distToPath = FVector3.Distance(projection, obstacle.Position);

                        if (distToPath < obstacle.Radius + _avoidanceRadius)
                        {
                            // Avoidance force (perpendicular to velocity)
                            FVector3 avoidanceDir = (boid.Position - obstacle.Position).Normalized;
                            force += avoidanceDir * (_lookAheadDistance / dist);
                            count++;
                        }
                    }
                }
            }

            if (count > 0)
            {
                return force.Normalized * Weight;
            }

            return FVector3.Zero;
        }
    }
}
