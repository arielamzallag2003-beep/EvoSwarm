using System;
using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IFormation
    {
        string Name { get; }
        int MaxSlots { get; }

        FVector3 GetSlotOffset(int slotIndex, int totalUnits, float spacing);
        FVector3 GetWorldPosition(FVector3 localOffset, FVector3 leaderPosition, FVector3 leaderForward);
    }

    public interface IFormationController
    {
        IFormation CurrentFormation { get; set; }
        IBoid Leader { get; set; }
        float Spacing { get; set; }
        bool IsActive { get; set; }

        IReadOnlyDictionary<IBoid, int> SlotAssignments { get; }

        void AssignSlots(IReadOnlyList<IBoid> boids);
        void ReassignSlots();
        void ReleaseSlot(IBoid boid);

        int? GetSlotIndex(IBoid boid);
        FVector3? GetTargetPosition(IBoid boid);

        event Action OnFormationChanged;
    }
}
