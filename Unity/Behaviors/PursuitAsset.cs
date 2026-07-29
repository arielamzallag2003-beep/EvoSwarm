using UnityEngine;
using PalaceOfFantasy.FlockForge.Core.Behaviors;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Config;

namespace PalaceOfFantasy.FlockForge.Unity.Behaviors
{
    [CreateAssetMenu(menuName = "FlockForge/Behaviors/Pursuit")]
    public class PursuitAsset : BehaviourAsset
    {
        public override IBehaviour CreateBehaviour()
            => new PursuitBehaviour { Weight = _weight, IsEnabled = _isEnabled };
    }
}
