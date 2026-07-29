namespace PalaceOfFantasy.FlockForge.Core.Behaviors
{
    public class SeekBehaviour : IBehaviour
    {
        public string Name => "Seek";
        public bool IsEnabled { get; set; } = true;
        public float Weight { get; set; } = 1f;
        public int Priority => 0;

        public FVector3 CalculateForce(IBoidContext context)
        {
            if (context.SeekTarget == null) return FVector3.Zero;
            var desired = (context.SeekTarget.Value - context.Self.Position).Normalized * context.Settings.MaxSpeed;
            return desired - context.Self.Velocity;
        }
    }
}
