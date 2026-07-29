using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Extensions
{
    public static class FlockForgeExtensions
    {
        public static FVector3 ToFVector3(this Vector3 v) => new(v.x, v.y, v.z);
        public static Vector3 ToUnityVector3(this FVector3 v) => new(v.X, v.Y, v.Z);

        public static Color ToUnityColor(this DebugColor c) => new(c.R, c.G, c.B, c.A);
    }
}
