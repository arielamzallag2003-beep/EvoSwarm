using UnityEngine;

namespace PalaceOfFantasy.FlockForge.Unity.Components
{
    /// <summary>
    /// Simple procedural animation for fish swimming.
    /// Oscillates the rotation slightly to simulate tail movement.
    /// </summary>
    public class ProceduralSwim : MonoBehaviour
    {
        [SerializeField] private float _frequency = 10f;
        [SerializeField] private float _amplitude = 15f;
        [SerializeField] private Transform _modelTransform;

        private float _randomOffset;

        private void Start()
        {
            _randomOffset = Random.value * Mathf.PI * 2;
            if (_modelTransform == null)
            {
                // Try to find child named "Model" as configured in the generator
                _modelTransform = transform.Find("Model");
                
                // If not found, use the first child if any, or this transform
                if (_modelTransform == null && transform.childCount > 0)
                    _modelTransform = transform.GetChild(0);
            }
        }

        private void Update()
        {
            if (_modelTransform == null) return;

            // Oscillate Y rotation around its current base rotation
            float wave = Mathf.Sin(Time.time * _frequency + _randomOffset) * _amplitude;
            
            // We use localRotation with Y=180 as base (set in generator)
            _modelTransform.localRotation = Quaternion.Euler(0, 180f + wave, 0);
        }

        public void SetFrequency(float freq) => _frequency = freq;
        public void SetAmplitude(float amp) => _amplitude = amp;
    }
}
