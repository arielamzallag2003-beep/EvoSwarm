using UnityEngine;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Food target that prey can eat.
    /// When eaten, spawns a new prey and respawns at a new location.
    /// </summary>
    public class FoodTarget : MonoBehaviour
    {
        [SerializeField] private float _eatDistance = 2f;
        [SerializeField] private LayerMask _preyLayer = -1;
        [SerializeField] private float _totalNutrition = 1.0f;
        [SerializeField] private float _biteNutrition = 0.25f;
        [SerializeField] private float _biteCooldown = 0.3f; // Faster eating = less time vulnerable

        private float _remainingNutrition;
        private float _lastBiteTime;
        private Vector3 _originalScale;

        private void Start()
        {
            _remainingNutrition = _totalNutrition;
            _originalScale = transform.localScale;
        }

        private void Update()
        {
            if (_remainingNutrition <= 0) return;
            if (Time.time - _lastBiteTime < _biteCooldown) return;
            
            // Check for nearby prey
            Collider[] nearby = Physics.OverlapSphere(transform.position, _eatDistance, _preyLayer);
            foreach (var col in nearby)
            {
                var hunger = col.GetComponent<PreyHunger>();
                if (hunger != null && hunger.IsSeekingFood)
                {
                    OnBite(hunger);
                    break;
                }
            }
        }

        private void OnBite(PreyHunger prey)
        {
            _lastBiteTime = Time.time;
            
            // Give nutrition to prey
            prey.TakeBite(_biteNutrition);
            
            // Notify manager for fitness and reproduction
            if (EcosystemManager.Instance != null)
            {
                EcosystemManager.Instance.OnFoodBite(prey.gameObject);
            }
            
            // Deplete food
            _remainingNutrition -= _biteNutrition;

            // Visual feedback: Shrink
            transform.localScale = _originalScale * (_remainingNutrition / _totalNutrition);

            if (_remainingNutrition <= 0)
            {
                OnDepleted(prey.gameObject);
            }
        }

        private void OnDepleted(GameObject lastEater)
        {
            if (EcosystemManager.Instance != null)
            {
                EcosystemManager.Instance.OnFoodEaten(gameObject, lastEater);
            }
            else
            {
                Destroy(gameObject);
            }
        }

        private void OnDrawGizmosSelected()
        {
            Gizmos.color = Color.green;
            Gizmos.DrawWireSphere(transform.position, _eatDistance);
        }
    }
}
