using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Core
{
    public class BoidContext : IBoidContext
    {
        public IBoid Self { get; set; }
        public IBoidSettings Settings => Self?.Settings;
        public IReadOnlyList<IBoid> Neighbors { get; set; }
        public IFlock Flock { get; set; }
        public float DeltaTime { get; set; }
        public float TotalTime { get; set; }
        public FVector3? SeekTarget { get; set; }
        public IBoid SeekTargetBoid { get; set; }
        public IReadOnlyList<FVector3> Threats { get; set; }
        public bool IsInFormation { get; set; }
        public IBoid Leader { get; set; }
        public FVector3? FormationSlot { get; set; }
        public IReadOnlyList<IObstacle> NearbyObstacles { get; set; }

        private static readonly List<IBoid> EmptyBoids = new();
        private static readonly List<FVector3> EmptyThreats = new();
        private static readonly List<IObstacle> EmptyObstacles = new();

        public BoidContext()
        {
            Neighbors = EmptyBoids;
            Threats = EmptyThreats;
            NearbyObstacles = EmptyObstacles;
        }

        public void Reset()
        {
            Self = null;
            Neighbors = EmptyBoids;
            Flock = null;
            DeltaTime = 0;
            TotalTime = 0;
            SeekTarget = null;
            SeekTargetBoid = null;
            Threats = EmptyThreats;
            IsInFormation = false;
            Leader = null;
            FormationSlot = null;
            NearbyObstacles = EmptyObstacles;
        }
    }
}
