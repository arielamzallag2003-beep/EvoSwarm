using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Core.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Config;

namespace PalaceOfFantasy.FlockForge.Unity.Behaviors
{
    [CreateAssetMenu(menuName = "FlockForge/Behaviors/Separation")]
    public class SeparationAsset : BehaviourAsset
    {
        public float radius = 2f;

        public override IBehaviour CreateBehaviour()
            => new SeparationBehaviour { Weight = _weight, Radius = radius, IsEnabled = _isEnabled };
    }
}
