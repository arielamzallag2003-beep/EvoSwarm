using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Config.AI.Transitions
{
    [CreateAssetMenu(menuName = "FlockForge/AI/Transitions/Distance")]
    public class DistanceTransitionAsset : TransitionAsset
    {
        public enum ComparisonType { Greater, Less }

        [SerializeField] private float _distance = 5f;
        [SerializeField] private ComparisonType _comparison = ComparisonType.Less;

        public override bool Evaluate(IBoid boid, IBoidContext context)
        {
            if (context.SeekTarget == null) return false;

            float sqrDist = FVector3.SqrDistance(boid.Position, context.SeekTarget.Value);
            float thresholdSqr = _distance * _distance;

            if (_comparison == ComparisonType.Less)
                return sqrDist < thresholdSqr;
            else
                return sqrDist > thresholdSqr;
        }
    }
}
