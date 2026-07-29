using UnityEngine;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Attached to predator boids. Manages their lifespan.
    /// Predators die after a set time unless they eat prey.
    /// Eating prey resets the lifespan and spawns a new predator.
    /// </summary>
    public class PredatorLifespan : MonoBehaviour
    {
        [SerializeField] private float _maxLifespan = 30f;
        private float _remainingLife;
        private bool _isDead = false;

        public float HungerRate { get; set; } = 1.0f;

        public float RemainingLife => _remainingLife;
        public float LifespanPercent => _remainingLife / _maxLifespan;

        public void Initialize(float lifespan)
        {
            _maxLifespan = lifespan;
            _remainingLife = lifespan;
        }

        private void Start()
        {
            if (_remainingLife <= 0)
            {
                _remainingLife = _maxLifespan;
            }
        }

        private void Update()
        {
            if (_isDead) return;

            _remainingLife -= Time.deltaTime * HungerRate;

            if (_remainingLife <= 0)
            {
                Die();
            }
        }

        public void ResetLifespan()
        {
            _remainingLife = _maxLifespan;
        }

        private void Die()
        {
            _isDead = true;
            
            if (EcosystemManager.Instance != null)
            {
                EcosystemManager.Instance.OnPredatorDied(gameObject);
            }
            else
            {
                Destroy(gameObject);
            }
        }
    }
}
