namespace PalaceOfFantasy.FlockForge.Core.Behaviors
{
    public class AlignmentBehaviour : IBehaviour
    {
        public string Name => "Alignment";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority => 0;

        public FVector3 CalculateForce(IBoidContext context)
        {
            if (context.Neighbors.Count == 0) return FVector3.Zero;

            FVector3 avgVel = FVector3.Zero;
            foreach (var n in context.Neighbors) avgVel += n.Velocity;
            avgVel /= context.Neighbors.Count;

            var desired = avgVel.Normalized * context.Settings.MaxSpeed;
            return desired - context.Self.Velocity;
        }
    }
}
