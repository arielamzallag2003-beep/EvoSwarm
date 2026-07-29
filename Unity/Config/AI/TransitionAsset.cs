using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Config.AI
{
    public abstract class TransitionAsset : ScriptableObject
    {
        public abstract bool Evaluate(IBoid boid, IBoidContext context);
    }
}
