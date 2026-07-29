namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IBoidSettings
    {
        float MaxSpeed { get; }
        float MaxForce { get; }
        float Mass { get; }
        float Drag { get; }
        MovementPlane MovementPlane { get; }

        float PerceptionRadius { get; }
        float FieldOfViewAngle { get; }
        int MaxNeighbors { get; }

        float ObstacleAvoidanceDistance { get; }
    }
}
