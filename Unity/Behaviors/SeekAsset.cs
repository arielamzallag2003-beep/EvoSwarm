using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Core.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Config;

namespace PalaceOfFantasy.FlockForge.Unity.Behaviors
{
    [CreateAssetMenu(menuName = "FlockForge/Behaviors/Seek")]
    public class SeekAsset : BehaviourAsset
    {
        public override IBehaviour CreateBehaviour()
            => new SeekBehaviour { Weight = _weight, IsEnabled = _isEnabled };
    }
}
