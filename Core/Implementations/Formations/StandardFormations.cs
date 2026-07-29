using System;
using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Extensions;

namespace PalaceOfFantasy.FlockForge.Runtime.Behaviors
{
    public class CircleFormation : IFormation
    {
        public string Name => "Circle";
        public int MaxSlots => 100;

        public FVector3 GetSlotOffset(int slotIndex, int totalUnits, float spacing)
        {
            float angle = (2f * Mathf.PI * slotIndex) / Math.Max(1, totalUnits);
            float radius = (totalUnits * spacing) / (2f * Mathf.PI);
            
            return new FVector3(Mathf.Cos(angle) * radius, 0, Mathf.Sin(angle) * radius);
        }

        public FVector3 GetWorldPosition(FVector3 localOffset, FVector3 leaderPosition, FVector3 leaderForward)
        {
            // Simple world space conversion
            return leaderPosition + localOffset;
        }
    }

    public class WedgeFormation : IFormation
    {
        public string Name => "Wedge";
        public int MaxSlots => 100;

        public FVector3 GetSlotOffset(int slotIndex, int totalUnits, float spacing)
        {
            int row = (int)Math.Floor(Math.Sqrt(slotIndex + 1));
            int col = slotIndex - row * row;
            
            float x = (col - row) * spacing;
            float z = -row * spacing;
            
            return new FVector3(x, 0, z);
        }

        public FVector3 GetWorldPosition(FVector3 localOffset, FVector3 leaderPosition, FVector3 leaderForward)
        {
            // Rotate local offset by leader forward
            Quaternion rotation = Quaternion.LookRotation(leaderForward.ToUnityVector3());
            return leaderPosition + (rotation * localOffset.ToUnityVector3()).ToFVector3();
        }
    }
}
