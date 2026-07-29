using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Core.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Config;

namespace PalaceOfFantasy.FlockForge.Unity.Behaviors
{
    [CreateAssetMenu(menuName = "FlockForge/Behaviors/Flee")]
    public class FleeAsset : BehaviourAsset
    {
        public float panicDistance = 10f;

        public override IBehaviour CreateBehaviour()
            => new FleeBehaviour { Weight = _weight, PanicDistance = panicDistance, IsEnabled = _isEnabled };
    }
}
