namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IObstacle
    {
        FVector3 Position { get; }
        float Radius { get; }
        bool IsActive { get; }
    }
}
 