using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Core.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Config;

namespace PalaceOfFantasy.FlockForge.Unity.Behaviors
{
    [CreateAssetMenu(menuName = "FlockForge/Behaviors/Arrival")]
    public class ArrivalAsset : BehaviourAsset
    {
        public float slowingRadius = 5f;

        public override IBehaviour CreateBehaviour()
            => new ArrivalBehaviour 
            { 
                Weight = _weight, 
                IsEnabled = _isEnabled,
                SlowingRadius = slowingRadius 
            };
    }
}
