using UnityEngine;
using PalaceOfFantasy.FlockForge.Unity.Components;
using PalaceOfFantasy.FlockForge.Unity.Config;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Extensions;
using System.Reflection;
using System.Linq;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Applies evolved genes from BoidBrain to the boid's actual behavior.
    /// Modifies speed, behavior weights, and perception based on genes.
    /// </summary>
    [RequireComponent(typeof(BoidBrain))]
    [RequireComponent(typeof(BoidAgent))]
    public class BoidGeneApplicator : MonoBehaviour
    {
        private BoidBrain _brain;
        private BoidAgent _agent;
        private BoidProfile _originalProfile;
        private BoidProfile _modifiedProfile;
        private FieldInfo _speedField;
        private FieldInfo _radiusField;
        
        [SerializeField] private bool _isPrey = true;

        private void Start()
        {
            _brain = GetComponent<BoidBrain>();
            _agent = GetComponent<BoidAgent>();
            
            if (_brain == null || _agent == null) return;
            
            _speedField = typeof(BoidProfile).GetField("_maxSpeed", BindingFlags.NonPublic | BindingFlags.Instance);
            _radiusField = typeof(BoidProfile).GetField("_perceptionRadius", BindingFlags.NonPublic | BindingFlags.Instance);

            // Clone the profile so we can modify it per-boid
            _originalProfile = _agent.Settings as BoidProfile;
            if (_originalProfile == null) return;
            
            _modifiedProfile = ScriptableObject.Instantiate(_originalProfile);
            ApplyGenes();
            SetProfileOnAgent(_modifiedProfile);
            ApplyWeightsToBehaviours();
        }

        private void Update()
        {
            if (this == null || _agent == null || _modifiedProfile == null || _brain == null) return;

            float sprintMultiplier = 1.0f;
            float targetDist = float.MaxValue;

            // Check for "Lunge" (Predator) or "Sprint" (Prey)
            if (!_isPrey)
            {
                var targetBoid = _agent.TargetProvider?.GetSeekTargetBoid(_agent);
                if (IsBoidValid(targetBoid))
                {
                    targetDist = Vector3.Distance(transform.position, targetBoid.Position.ToUnityVector3());
                    if (targetDist < 8f) // Start lunging when close
                    {
                        // Lunge boost: 1.0 to 1.8x based on Aggression and proximity
                        float proximityFactor = 1.0f - Mathf.Clamp01(targetDist / 8f);
                        sprintMultiplier += _brain.Aggression * 0.4f * proximityFactor;
                    }
                }
            }
            else
            {
                // Prey panic sprint: increase speed if threats are very close
                var threats = _agent.TargetProvider?.GetThreats(_agent);
                if (threats != null && threats.Count > 0)
                {
                    float minThreatDist = float.MaxValue;
                    foreach (var t in threats)
                    {
                        float d = Vector3.Distance(transform.position, t.ToUnityVector3());
                        if (d < minThreatDist) minThreatDist = d;
                    }

                    if (minThreatDist < 6f)
                    {
                        float panicFactor = 1.0f - Mathf.Clamp01(minThreatDist / 6f);
                        sprintMultiplier += 0.5f * panicFactor;
                    }
                }
            }

            // Apply calculated speed to the modified profile
            float baseModifiedSpeed = _originalProfile.MaxSpeed * _brain.SpeedModifier;
            float currentMaxSpeed = baseModifiedSpeed * sprintMultiplier;
            
            if (_speedField != null)
            {
                _speedField.SetValue(_modifiedProfile, currentMaxSpeed);
            }

            // High metabolic cost for sprinting
            if (!_isPrey)
            {
                var lifespan = GetComponent<PredatorLifespan>();
                if (lifespan != null)
                {
                    float baseHunger = Mathf.Pow(_brain.SpeedModifier, 2f);
                    // Sprinting is 3x more exhausting
                    lifespan.HungerRate = baseHunger * (sprintMultiplier > 1.1f ? 3.0f : 1.0f);
                }
            }
        }

        private bool IsBoidValid(IBoid boid)
        {
            if (boid == null) return false;
            if (boid is MonoBehaviour mb) return mb != null;
            return true;
        }

        private void ApplyGenes()
        {
            if (_brain == null || _modifiedProfile == null) return;
            
            // Apply speed modifier from genes
            float baseSpeed = _originalProfile.MaxSpeed;
            float modifiedSpeed = baseSpeed * _brain.SpeedModifier;
            
            // Use reflection to set the private field
            if (_speedField != null)
            {
                _speedField.SetValue(_modifiedProfile, modifiedSpeed);
            }
            
            // Apply perception radius
            if (_radiusField != null)
            {
                _radiusField.SetValue(_modifiedProfile, _brain.PerceptionRadius);
            }

            // Apply metabolic cost to predator
            if (!_isPrey)
            {
                var lifespan = GetComponent<PredatorLifespan>();
                if (lifespan != null)
                {
                    // Hunger rate is non-linear relative to speed modifier
                    // Speed 1.0 -> Hunger 1.0
                    // Speed 2.0 -> Hunger 4.0
                    lifespan.HungerRate = Mathf.Pow(_brain.SpeedModifier, 2f);
                }
            }
        }

        private void ApplyWeightsToBehaviours()
        {
            if (_agent == null || _brain == null) return;

            // Search in runtime behaviours
            foreach (var behavior in _agent.RuntimeBehaviours)
            {
                UpdateBehaviorWeight(behavior);

                // Recursively check state machines
                if (behavior is FlockStateMachineBehaviour stateMachineBehaviour)
                {
                    // Force machine creation for this boid if it doesn't exist
                    // This is safe because BoidGeneApplicator runs in Start or later
                    var machine = stateMachineBehaviour.GetMachine(_agent.Id);
                    if (machine != null)
                    {
                        foreach (var state in machine.States)
                        {
                            foreach (var b in state.Behaviours)
                            {
                                UpdateBehaviorWeight(b);
                            }
                        }
                    }
                }
            }
        }

        private void UpdateBehaviorWeight(IBehaviour behavior)
        {
            if (behavior == null) return;

            string name = behavior.Name.ToLower();
            float multiplier = 1.0f;

            if (_isPrey)
            {
                if (name.Contains("flee")) multiplier = _brain.FleeWeight;
                else if (name.Contains("separation")) multiplier = _brain.SeparationWeight;
                else if (name.Contains("cohesion")) multiplier = _brain.CohesionWeight;
            }
            else
            {
                if (name.Contains("seek")) multiplier = _brain.SeekWeight;
                else if (name.Contains("separation")) multiplier = _brain.SeparationWeight;
                else if (name.Contains("cohesion")) multiplier = _brain.CohesionWeight;
            }

            behavior.Weight *= multiplier;
        }

        private void SetProfileOnAgent(BoidProfile profile)
        {
            var profileField = typeof(BoidAgent).GetField("_profile", BindingFlags.NonPublic | BindingFlags.Instance);
            if (profileField != null)
            {
                profileField.SetValue(_agent, profile);
            }
        }

        /// <summary>
        /// Get the flee weight multiplier for behaviors to use
        /// </summary>
        public float GetFleeWeightMultiplier()
        {
            return _isPrey && _brain != null ? _brain.FleeWeight : 1f;
        }

        /// <summary>
        /// Get the seek weight multiplier for behaviors to use
        /// </summary>
        public float GetSeekWeightMultiplier()
        {
            return !_isPrey && _brain != null ? _brain.SeekWeight : 1f;
        }

        /// <summary>
        /// Get the separation weight multiplier
        /// </summary>
        public float GetSeparationMultiplier()
        {
            return _brain != null ? _brain.SeparationWeight : 1f;
        }

        /// <summary>
        /// Get the cohesion weight multiplier
        /// </summary>
        public float GetCohesionMultiplier()
        {
            return _brain != null ? _brain.CohesionWeight : 1f;
        }

        /// <summary>
        /// Get panic distance for flee behavior
        /// </summary>
        public float GetPanicDistance()
        {
            return _isPrey && _brain != null ? _brain.PanicDistance : 10f;
        }

        private void OnDestroy()
        {
            // Clean up the cloned profile
            if (_modifiedProfile != null)
            {
                Destroy(_modifiedProfile);
            }
        }
    }
}
