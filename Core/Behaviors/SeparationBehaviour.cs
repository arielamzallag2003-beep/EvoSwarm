namespace PalaceOfFantasy.FlockForge.Core.Behaviors
{
    public class SeparationBehaviour : IBehaviour
    {
        public string Name => "Separation";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1.5f;
        public int Priority => 0;
        public float Radius { get; set; } = 2f;

        public FVector3 CalculateForce(IBoidContext context)
        {
            if (context.Neighbors.Count == 0) return FVector3.Zero;

            FVector3 force = FVector3.Zero;
            foreach (var n in context.Neighbors)
            {
                var diff = context.Self.Position - n.Position;
                var dist = diff.Magnitude;
                if (dist < Radius && dist > 0.001f)
                {
                    force += diff.Normalized / dist;
                }
            }
            var desired = force.Normalized * context.Settings.MaxSpeed;
            return desired - context.Self.Velocity;
        }
    }
}
