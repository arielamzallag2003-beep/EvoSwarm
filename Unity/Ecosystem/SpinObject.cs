using UnityEngine;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Simple spinning animation for food objects to make them more visually appealing.
    /// </summary>
    public class SpinObject : MonoBehaviour
    {
        [SerializeField] private Vector3 _rotationSpeed = new Vector3(0, 45, 15);
        [SerializeField] private float _bobAmplitude = 0.1f;
        [SerializeField] private float _bobFrequency = 1.5f;
        
        private Vector3 _startPosition;
        private float _bobOffset;

        private void Start()
        {
            _startPosition = transform.position;
            _bobOffset = Random.value * Mathf.PI * 2; // Random phase offset
        }

        private void Update()
        {
            // Spin
            transform.Rotate(_rotationSpeed * Time.deltaTime, Space.World);
            
            // Bob up and down
            float bobY = Mathf.Sin((Time.time * _bobFrequency) + _bobOffset) * _bobAmplitude;
            transform.position = _startPosition + new Vector3(0, bobY, 0);
        }
        
        // Update start position if food is repositioned
        public void ResetPosition(Vector3 newPosition)
        {
            _startPosition = newPosition;
        }
    }
}
