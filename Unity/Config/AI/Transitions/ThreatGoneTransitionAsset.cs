using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Config.AI.Transitions
{
    /// <summary>
    /// Returns true if NO threat is within the specified safe distance.
    /// Used to transition back to a calm state after fleeing.
    /// </summary>
    [CreateAssetMenu(menuName = "FlockForge/AI/Transitions/Threat Gone")]
    public class ThreatGoneTransitionAsset : TransitionAsset
    {
        [SerializeField] private float _safeDistance = 15f;

        public override bool Evaluate(IBoid boid, IBoidContext context)
        {
            if (context.Threats == null || context.Threats.Count == 0) return true;

            float thresholdSqr = _safeDistance * _safeDistance;
            foreach (var threat in context.Threats)
            {
                if (FVector3.SqrDistance(boid.Position, threat) < thresholdSqr)
                    return false; // A threat is still nearby
            }
            return true; // All threats are far away
        }
    }
}
