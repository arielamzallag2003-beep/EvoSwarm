using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Runtime.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Components;

namespace PalaceOfFantasy.FlockForge.Unity.Config.AI
{
    [CreateAssetMenu(fileName = "FormationBehaviour", menuName = "FlockForge/AI/Behaviors/Formation")]
    public class FormationBehaviourAsset : BehaviourAsset
    {
        [SerializeField] private float _arrivalRadius = 2.0f;

        public override IBehaviour CreateBehaviour()
        {
            // Note: In a real scenario, we'd need to find the UnityFormationController.
            // Since this is created at runtime per-boid or per-state, the behavior 
            // will need to find its controller.
            return new FormationBehaviour(_weight, null, _arrivalRadius);
        }
    }
}
