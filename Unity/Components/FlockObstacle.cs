using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Extensions;

namespace PalaceOfFantasy.FlockForge.Unity.Components
{
    /// <summary>
    /// Component that registers a GameObject as an obstacle for boids.
    /// </summary>
    public class FlockObstacle : MonoBehaviour, IObstacle
    {
        [SerializeField] private float _radius = 1.0f;
        [SerializeField] private bool _isActive = true;

        public FVector3 Position => transform.position.ToFVector3();
        public float Radius => _radius * Mathf.Max(transform.localScale.x, transform.localScale.y, transform.localScale.z);
        public bool IsActive => _isActive && gameObject.activeInHierarchy;

        private void OnDrawGizmosSelected()
        {
            Gizmos.color = new Color(1, 0, 0, 0.3f);
            Gizmos.DrawWireSphere(transform.position, Radius);
        }
    }
}
