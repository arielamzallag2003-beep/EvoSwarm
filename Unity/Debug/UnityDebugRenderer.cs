using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Extensions;

namespace PalaceOfFantasy.FlockForge.Unity.Debug
{
    public class UnityDebugRenderer : MonoBehaviour, IDebugRenderer
    {
        [SerializeField] private bool _isEnabled = true;
        
        public bool IsEnabled 
        { 
            get => _isEnabled; 
            set => _isEnabled = value; 
        }

        public void DrawLine(FVector3 from, FVector3 to, DebugColor color)
        {
            if (!IsEnabled) return;
            UnityEngine.Debug.DrawLine(from.ToUnityVector3(), to.ToUnityVector3(), color.ToUnityColor());
        }

        public void DrawSphere(FVector3 center, float radius, DebugColor color)
        {
            if (!IsEnabled) return;
            // Draw sphere using 3 circles (simplified Gizmo-like behavior)
             // Using debug lines for runtime visualization
        }

        public void DrawCone(FVector3 origin, FVector3 direction, float angle, float length, DebugColor color)
        {
            if (!IsEnabled) return;
            var unityOrigin = origin.ToUnityVector3();
            var unityDir = direction.ToUnityVector3();
            UnityEngine.Debug.DrawRay(unityOrigin, unityDir * length, color.ToUnityColor());
        }

        public void DrawText(FVector3 position, string text, DebugColor color)
        {
            // Unity debug doesn't support text in runtime easily without Gizmos or Custom UI
        }

        private void OnDrawGizmos()
        {
            // In a real implementation this would queue draw calls to be rendered here
            // or use Gizmos directly if IDebugRenderer exposes a "DrawGizmos" method
        }
    }
}
