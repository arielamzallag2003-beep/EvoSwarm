namespace PalaceOfFantasy.FlockForge.Core.Behaviors
{
    public class FleeBehaviour : IBehaviour
    {
        public string Name => "Flee";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 2f;
        public int Priority => 8;
        public float PanicDistance { get; set; } = 5f;

        public FVector3 CalculateForce(IBoidContext context)
        {
            if (context.Threats.Count == 0) return FVector3.Zero;

            FVector3 force = FVector3.Zero;
            foreach (var threat in context.Threats)
            {
                var diff = context.Self.Position - threat;
                if (diff.Magnitude < PanicDistance)
                {
                    force += diff.Normalized * (PanicDistance - diff.Magnitude);
                }
            }
            return force.Normalized * context.Settings.MaxSpeed;
        }
    }
}
