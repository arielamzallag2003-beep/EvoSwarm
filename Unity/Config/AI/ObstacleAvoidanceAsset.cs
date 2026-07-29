using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Runtime.Behaviors;

namespace PalaceOfFantasy.FlockForge.Unity.Config.AI
{
    [CreateAssetMenu(fileName = "ObstacleAvoidance", menuName = "FlockForge/AI/Behaviors/Obstacle Avoidance")]
    public class ObstacleAvoidanceAsset : BehaviourAsset
    {
        [SerializeField] private float _avoidanceRadius = 2.0f;
        [SerializeField] private float _lookAheadDistance = 5.0f;

        public override IBehaviour CreateBehaviour()
        {
            return new ObstacleAvoidanceBehaviour(_weight, _avoidanceRadius, _lookAheadDistance);
        }
    }
}
