using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Core.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Config;

namespace PalaceOfFantasy.FlockForge.Unity.Behaviors
{
    [CreateAssetMenu(menuName = "FlockForge/Behaviors/Wander")]
    public class WanderAsset : BehaviourAsset
    {
        [Tooltip("Strength of the jitter")]
        public float jitter = 5f;
        
        [Tooltip("Radius of the wander circle")]
        public float radius = 2f;
        
        [Tooltip("Distance of the wander circle")]
        public float distance = 3f;

        public override IBehaviour CreateBehaviour()
            => new WanderBehaviour 
            { 
                Weight = _weight, 
                IsEnabled = _isEnabled,
                Jitter = jitter,
                Radius = radius,
                Distance = distance
            };
    }
}
