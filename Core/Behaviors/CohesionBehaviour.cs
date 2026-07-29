namespace PalaceOfFantasy.FlockForge.Core.Behaviors
{
    public class CohesionBehaviour : IBehaviour
    {
        public string Name => "Cohesion";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority => 0;

        public FVector3 CalculateForce(IBoidContext context)
        {
            if (context.Neighbors.Count == 0) return FVector3.Zero;

            var center = FVector3.Zero;
            foreach (var n in context.Neighbors) center += n.Position;
            center /= context.Neighbors.Count;

            var direction = center - context.Self.Position;
            var desired = direction.Normalized * context.Settings.MaxSpeed;
            return desired - context.Self.Velocity;
        }
    }
} 
