using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IBoidContext
    {
        IBoid Self { get; }
        IBoidSettings Settings { get; }

        IReadOnlyList<IBoid> Neighbors { get; }
        IFlock Flock { get; }

        float DeltaTime { get; }
        float TotalTime { get; }

        FVector3? SeekTarget { get; }
        IBoid SeekTargetBoid { get; }
        IReadOnlyList<FVector3> Threats { get; }

        bool IsInFormation { get; }
        IBoid Leader { get; }
        FVector3? FormationSlot { get; }

        IReadOnlyList<IObstacle> NearbyObstacles { get; }
    }
}
