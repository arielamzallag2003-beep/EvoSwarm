using System;
using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Extensions;

namespace PalaceOfFantasy.FlockForge.Runtime.Behaviors
{
    public class LineFormation : IFormation
    {
        public string Name => "Line";
        public int MaxSlots => 100;

        public FVector3 GetSlotOffset(int slotIndex, int totalUnits, float spacing)
        {
            float center = (totalUnits - 1) * spacing / 2f;
            return new FVector3(slotIndex * spacing - center, 0, 0);
        }

        public FVector3 GetWorldPosition(FVector3 localOffset, FVector3 leaderPosition, FVector3 leaderForward)
        {
            Quaternion rotation = Quaternion.LookRotation(leaderForward.ToUnityVector3());
            return leaderPosition + (rotation * localOffset.ToUnityVector3()).ToFVector3();
        }
    }
}
