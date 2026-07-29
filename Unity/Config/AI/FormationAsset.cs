using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Runtime.Behaviors;

namespace PalaceOfFantasy.FlockForge.Unity.Config.AI
{
    public enum FormationType { Circle, Wedge, Line }

    [CreateAssetMenu(fileName = "Formation", menuName = "FlockForge/AI/Formation")]
    public class FormationAsset : ScriptableObject
    {
        [SerializeField] private FormationType _type;

        public IFormation CreateFormation()
        {
            switch (_type)
            {
                case FormationType.Circle: return new CircleFormation();
                case FormationType.Wedge: return new WedgeFormation();
                default: return new CircleFormation();
            }
        }
    }
}
