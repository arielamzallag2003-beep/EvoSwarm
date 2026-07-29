using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Config
{
    public abstract class BehaviourAsset : ScriptableObject
    {
        [SerializeField] protected float _weight = 1f;
        [SerializeField] protected bool _isEnabled = true;

        public abstract IBehaviour CreateBehaviour();
    }
}
