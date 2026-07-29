using UnityEngine;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Attached to prey boids. Allows them to be caught by predators.
    /// When caught, the prey dies and the predator reproduces.
    /// </summary>
    public class PreyCatchable : MonoBehaviour
    {
        [SerializeField] private float _catchDistance = 1.5f;
        [SerializeField] private LayerMask _predatorLayer = -1;
        
        private bool _isCaught = false;

        private void Update()
        {
            if (_isCaught) return;
            
            // Check for nearby predators
            Collider[] nearby = Physics.OverlapSphere(transform.position, _catchDistance, _predatorLayer);
            foreach (var col in nearby)
            {
                var lifespan = col.GetComponent<PredatorLifespan>();
                if (lifespan != null)
                {
                    OnCaught(col.gameObject);
                    break;
                }
            }
        }

        private void OnCaught(GameObject predator)
        {
            _isCaught = true;
            
            if (EcosystemManager.Instance != null)
            {
                EcosystemManager.Instance.OnPreyEaten(gameObject, predator);
            }
            else
            {
                // Fallback if no manager
                Destroy(gameObject);
            }
        }

        private void OnDrawGizmosSelected()
        {
            Gizmos.color = Color.red;
            Gizmos.DrawWireSphere(transform.position, _catchDistance);
        }
    }
}
