using UnityEngine;
using PalaceOfFantasy.FlockForge.Unity.Providers;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Extensions;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Target provider that finds the nearest prey for predators to hunt.
    /// Updates the seek target dynamically to the closest prey boid.
    /// </summary>
    public class NearestPreyTargetProvider : TransformTargetProvider
    {
        [SerializeField] private float _updateInterval = 0.2f;
        
        private float _lastUpdateTime;
        private IBoid _nearestPreyBoid;
        private GameObject _lastNearestPrey;

        public override FVector3? GetSeekTarget(IBoid boid) 
        {
            UpdateNearestPreyTarget();
            return _lastNearestPrey != null ? _lastNearestPrey.transform.position.ToFVector3() : (FVector3?)null;
        }

        public override IBoid GetSeekTargetBoid(IBoid boid) 
        {
            UpdateNearestPreyTarget();
            return _nearestPreyBoid;
        }

        private void UpdateNearestPreyTarget()
        {
            // Immediate invalidation if the current target was destroyed
            if (_lastNearestPrey == null)
            {
                _nearestPreyBoid = null;
            }

            if (Time.time - _lastUpdateTime < _updateInterval) return;
            _lastUpdateTime = Time.time;

            if (EcosystemManager.Instance == null) return;

            var nearestPrey = EcosystemManager.Instance.GetNearestPrey(transform.position);
            
            if (nearestPrey != null)
            {
                _lastNearestPrey = nearestPrey;
                _nearestPreyBoid = nearestPrey.GetComponent<IBoid>();
            }
            else
            {
                _lastNearestPrey = null;
                _nearestPreyBoid = null;
            }
        }
    }
}
