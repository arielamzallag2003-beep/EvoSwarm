using UnityEngine;
using System.Collections.Generic;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Extensions;

namespace PalaceOfFantasy.FlockForge.Unity.Providers
{
    public class TransformTargetProvider : MonoBehaviour, ITargetProvider
    {
        [SerializeField] private Transform _seekTarget;
        [SerializeField] private List<Transform> _threats = new();
        [SerializeField] private LayerMask _obstacleMask;


        public virtual FVector3? GetSeekTarget(IBoid boid)
        {
            if (_seekTarget == null) return null;
            return _seekTarget.position.ToFVector3();
        }

        public virtual IBoid GetSeekTargetBoid(IBoid boid)
        {
            if (_seekTarget == null) return null;
            // Try to get BoidAgent from the target or its parent/children
            return _seekTarget.GetComponentInParent<IBoid>() ?? _seekTarget.GetComponentInChildren<IBoid>();
        }

        public virtual IReadOnlyList<FVector3> GetThreats(IBoid boid)
        {
            var threatPositions = new List<FVector3>();
            foreach (var threat in _threats)
            {
                if (threat != null)
                    threatPositions.Add(threat.position.ToFVector3());
            }
            return threatPositions;
        }

        public virtual IReadOnlyList<IObstacle> GetNearbyObstacles(IBoid boid)
        {
            // Simple physics overlap implementation
            var obstacles = new List<IObstacle>();
            if (boid.Settings == null) return obstacles;
            
            var center = boid.Position.ToUnityVector3();
            var radius = boid.Settings.ObstacleAvoidanceDistance;

            var colliders = Physics.OverlapSphere(center, radius, _obstacleMask);
            foreach (var col in colliders)
            {
                if (col.transform == (boid as MonoBehaviour)?.transform) continue;
                obstacles.Add(new UnityObstacleWrapper(col));
            }
            
            return obstacles;
        }

        // Wrapper for Unity Collider to IObstacle
        private class UnityObstacleWrapper : IObstacle
        {
            private readonly Collider _collider;
            
            public UnityObstacleWrapper(Collider collider)
            {
                _collider = collider;
            }

            public FVector3 Position => _collider.transform.position.ToFVector3();
            // Simplified radius for generic colliders
            public float Radius => _collider.bounds.extents.magnitude; 
            public bool IsActive => _collider.gameObject.activeInHierarchy;
        }
    }
}
