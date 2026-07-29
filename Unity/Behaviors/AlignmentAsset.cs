using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Core.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Config;

namespace PalaceOfFantasy.FlockForge.Unity.Behaviors
{
    [CreateAssetMenu(menuName = "FlockForge/Behaviors/Alignment")]
    public class AlignmentAsset : BehaviourAsset
    {
        public override IBehaviour CreateBehaviour()
            => new AlignmentBehaviour { Weight = _weight, IsEnabled = _isEnabled };
    }
}
