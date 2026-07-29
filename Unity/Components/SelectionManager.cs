using UnityEngine;
using System.Collections.Generic;
using PalaceOfFantasy.FlockForge.Unity.Extensions;
using UnityEngine.InputSystem;

namespace PalaceOfFantasy.FlockForge.Unity.Components
{
    /// <summary>
    /// Handles RTS-style selection and move commands.
    /// </summary>
    public class SelectionManager : MonoBehaviour
    {
        [SerializeField] private LayerMask _selectionLayer;
        [SerializeField] private LayerMask _groundLayer;
        [SerializeField] private RectTransform _selectionBoxVisual;

        private List<BoidCommandAgent> _selectedBoids = new();
        private Vector2 _startMousePos;
        private bool _isDragging;

        private void Update()
        {
            HandleSelection();
            HandleCommands();
        }

        private void HandleSelection()
        {
            var mouse = Mouse.current;
            if (mouse == null) return;

            if (mouse.leftButton.wasPressedThisFrame)
            {
                _startMousePos = mouse.position.ReadValue();
                _isDragging = true;
            }

            if (_isDragging)
            {
                if (mouse.leftButton.wasReleasedThisFrame)
                {
                    _isDragging = false;
                    _selectionBoxVisual.gameObject.SetActive(false);
                    PerformSelection();
                }
                else
                {
                    UpdateSelectionBox(mouse.position.ReadValue());
                }
            }
        }

        private void UpdateSelectionBox(Vector2 currentMousePos)
        {
            if (!_selectionBoxVisual.gameObject.activeSelf)
                _selectionBoxVisual.gameObject.SetActive(true);

            float width = currentMousePos.x - _startMousePos.x;
            float height = currentMousePos.y - _startMousePos.y;

            _selectionBoxVisual.sizeDelta = new Vector2(Mathf.Abs(width), Mathf.Abs(height));
            _selectionBoxVisual.anchoredPosition = _startMousePos + new Vector2(width / 2, height / 2);
        }

        private void PerformSelection()
        {
            var mouse = Mouse.current;
            Vector2 endMousePos = mouse.position.ReadValue();

            // Clear previous selection if not holding shift
            if (!Keyboard.current.shiftKey.isPressed)
            {
                foreach (var boid in _selectedBoids)
                    boid.IsSelected = false;
                _selectedBoids.Clear();
            }

            // Single click vs Box selection
            if (Vector2.Distance(_startMousePos, endMousePos) < 5f)
            {
                Ray ray = Camera.main.ScreenPointToRay(_startMousePos);
                if (Physics.Raycast(ray, out RaycastHit hit, 1000f, _selectionLayer))
                {
                    var commandAgent = hit.collider.GetComponentInParent<BoidCommandAgent>();
                    if (commandAgent != null)
                        SelectBoid(commandAgent);
                }
            }
            else
            {
                // Box selection
                Rect selectionRect = new Rect(
                    Mathf.Min(_startMousePos.x, endMousePos.x),
                    Mathf.Min(_startMousePos.y, endMousePos.y),
                    Mathf.Abs(_startMousePos.x - endMousePos.x),
                    Mathf.Abs(_startMousePos.y - endMousePos.y)
                );

                var allCommandAgents = FindObjectsByType<BoidCommandAgent>(FindObjectsSortMode.None);
                foreach (var agent in allCommandAgents)
                {
                    Vector2 screenPos = Camera.main.WorldToScreenPoint(agent.transform.position);
                    if (selectionRect.Contains(screenPos))
                        SelectBoid(agent);
                }
            }
        }

        private void SelectBoid(BoidCommandAgent agent)
        {
            if (!_selectedBoids.Contains(agent))
            {
                _selectedBoids.Add(agent);
                agent.IsSelected = true;
            }
        }

        private void HandleCommands()
        {
            var mouse = Mouse.current;
            if (mouse == null || _selectedBoids.Count == 0) return;

            if (mouse.rightButton.wasPressedThisFrame)
            {
                Ray ray = Camera.main.ScreenPointToRay(mouse.position.ReadValue());
                if (Physics.Raycast(ray, out RaycastHit hit, 1000f, _groundLayer))
                {
                    foreach (var boid in _selectedBoids)
                    {
                        boid.GiveMoveOrder(hit.point);
                    }
                }
            }
        }
    }
}
