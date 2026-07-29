using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Runtime.Behaviors;
using PalaceOfFantasy.FlockForge.Unity.Extensions;

namespace PalaceOfFantasy.FlockForge.Unity.Components
{
    /// <summary>
    /// Component that allows a boid to receive RTS-style orders.
    /// </summary>
    public class BoidCommandAgent : MonoBehaviour
    {
        [SerializeField] private float _commandWeight = 50f; // High weight to override flocking
        [SerializeField] private bool _isSelected;
        
        private CommandBehaviour _commandBehaviour;
        private BoidAgent _agent;
        private GameObject _selectionVisual;

        public bool IsSelected 
        { 
            get => _isSelected;
            set 
            {
                _isSelected = value;
                if (_selectionVisual != null) _selectionVisual.SetActive(value);
            }
        }

        private void Awake()
        {
            _agent = GetComponent<BoidAgent>();
            _commandBehaviour = new CommandBehaviour(_commandWeight);
            
            // Register behavior with the agent
            if (_agent != null)
            {
                _agent.AddRuntimeBehaviour(_commandBehaviour);
            }

            CreateSelectionVisual();
        }

        private void CreateSelectionVisual()
        {
            _selectionVisual = GameObject.CreatePrimitive(PrimitiveType.Sphere);
            _selectionVisual.name = "SelectionHighlight";
            _selectionVisual.transform.SetParent(transform);
            _selectionVisual.transform.localPosition = new Vector3(0, -0.5f, 0);
            _selectionVisual.transform.localScale = new Vector3(1.2f, 0.1f, 1.2f);
            
            var renderer = _selectionVisual.GetComponent<Renderer>();
            var mat = new Material(Shader.Find("Sprites/Default"));
            mat.color = new Color(0, 1, 0, 0.5f);
            renderer.sharedMaterial = mat;
            
            GameObject.DestroyImmediate(_selectionVisual.GetComponent<Collider>());
            _selectionVisual.SetActive(_isSelected);
        }

        public void GiveMoveOrder(Vector3 destination)
        {
            _commandBehaviour.SetTarget(destination.ToFVector3());
        }

        public void Stop()
        {
            _commandBehaviour.SetTarget(null);
        }
    }
}
