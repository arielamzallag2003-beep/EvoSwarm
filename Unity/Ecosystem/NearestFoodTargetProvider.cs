using UnityEngine;
using PalaceOfFantasy.FlockForge.Unity.Providers;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Extensions;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Target provider that finds the nearest food for prey to seek.
    /// Updates the seek target dynamically to the closest food object.
    /// </summary>
    public class NearestFoodTargetProvider : TransformTargetProvider
    {
        [SerializeField] private float _updateInterval = 0.2f;
        
        private PreyHunger _hunger;
        private float _lastUpdateTime;
        private GameObject _lastNearestFood;

        public override FVector3? GetSeekTarget(IBoid boid) 
        {
            UpdateNearestFoodTarget();
            return _lastNearestFood != null ? _lastNearestFood.transform.position.ToFVector3() : (FVector3?)null;
        }

        public override IBoid GetSeekTargetBoid(IBoid boid) => null;

        private void Awake()
        {
            _hunger = GetComponent<PreyHunger>();
        }

        private void UpdateNearestFoodTarget()
        {
            if (Time.time - _lastUpdateTime < _updateInterval) return;
            _lastUpdateTime = Time.time;

            if (EcosystemManager.Instance == null) return;

            // Only seek food if hungry
            if (_hunger != null && !_hunger.IsSeekingFood)
            {
                _lastNearestFood = null;
                return;
            }

            var nearestFood = EcosystemManager.Instance.GetNearestFood(transform.position);
            _lastNearestFood = nearestFood;
        }
    }
}
