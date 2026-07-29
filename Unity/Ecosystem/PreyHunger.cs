using UnityEngine;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Manages hunger and satiety levels for prey.
    /// Controls whether the boid should actively seek food.
    /// </summary>
    public class PreyHunger : MonoBehaviour
    {
        [Header("Hunger Settings")]
        [SerializeField] private float _hungerRate = 0.05f; // Hunger increase per second
        [SerializeField] private float _hungerThreshold = 0.7f; // Seek food above this
        [SerializeField] private float _satietyThreshold = 0.2f; // Stop seeking below this
        
        [Header("Runtime State")]
        [SerializeField, Range(0, 1)] private float _hungerLevel = 0.5f;
        [SerializeField] private bool _isSeekingFood = false;

        public float HungerLevel => _hungerLevel;
        public bool IsSeekingFood => _isSeekingFood;

        private void Update()
        {
            // Increase hunger over time
            _hungerLevel = Mathf.Clamp01(_hungerLevel + _hungerRate * Time.deltaTime);

            // Logic to switch between seeking food and not
            if (!_isSeekingFood && _hungerLevel > _hungerThreshold)
            {
                _isSeekingFood = true;
            }
            else if (_isSeekingFood && _hungerLevel < _satietyThreshold)
            {
                _isSeekingFood = false;
            }
        }

        public void TakeBite(float nutritionValue)
        {
            _hungerLevel = Mathf.Clamp01(_hungerLevel - nutritionValue);
        }
    }
}
