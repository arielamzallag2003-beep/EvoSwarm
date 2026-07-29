using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Core.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Config;

namespace PalaceOfFantasy.FlockForge.Unity.Behaviors
{
    [CreateAssetMenu(menuName = "FlockForge/Behaviors/StayInRadius")]
    public class StayInRadiusAsset : BehaviourAsset
    {
        public float radius = 20f;

        public override IBehaviour CreateBehaviour()
            => new StayInRadiusBehaviour 
            { 
                Weight = _weight, 
                IsEnabled = _isEnabled,
                Radius = radius
            };
    }
}
