using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Config.AI.Transitions
{
    /// <summary>
    /// Returns true if any threat is within the specified distance.
    /// </summary>
    [CreateAssetMenu(menuName = "FlockForge/AI/Transitions/Threat Nearby")]
    public class ThreatNearbyTransitionAsset : TransitionAsset
    {
        [SerializeField] private float _panicDistance = 10f;

        public override bool Evaluate(IBoid boid, IBoidContext context)
        {
            if (context.Threats == null || context.Threats.Count == 0) return false;

            float thresholdSqr = _panicDistance * _panicDistance;
            foreach (var threat in context.Threats)
            {
                if (FVector3.SqrDistance(boid.Position, threat) < thresholdSqr)
                    return true;
            }
            return false;
        }
    }
}
