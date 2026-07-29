using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Core.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Config;

namespace PalaceOfFantasy.FlockForge.Unity.Behaviors
{
    [CreateAssetMenu(menuName = "FlockForge/Behaviors/Cohesion")]
    public class CohesionAsset : BehaviourAsset
    {
        public override IBehaviour CreateBehaviour()
        {
            return new CohesionBehaviour 
            { 
                Weight = _weight, 
                IsEnabled = _isEnabled 
            };
        }
    }
}
