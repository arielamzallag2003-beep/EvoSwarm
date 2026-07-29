using UnityEngine;
using System;
using System.Collections.Generic;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Components
{
    public class UnityFormationController : MonoBehaviour, IFormationController
    {
        [SerializeField] private float _spacing = 2.0f;
        [SerializeField] private bool _isActive = true;

        private IFormation _currentFormation;
        private IBoid _leader;
        private Dictionary<IBoid, int> _slotAssignments = new();

        public IFormation CurrentFormation 
        { 
            get => _currentFormation; 
            set { _currentFormation = value; OnFormationChanged?.Invoke(); } 
        }

        public IBoid Leader 
        { 
            get => _leader; 
            set => _leader = value; 
        }

        public float Spacing 
        { 
            get => _spacing; 
            set => _spacing = value; 
        }

        public bool IsActive 
        { 
            get => _isActive; 
            set => _isActive = value; 
        }

        public IReadOnlyDictionary<IBoid, int> SlotAssignments => _slotAssignments;

        public event Action OnFormationChanged;

        public void AssignSlots(IReadOnlyList<IBoid> boids)
        {
            _slotAssignments.Clear();
            for (int i = 0; i < boids.Count; i++)
            {
                _slotAssignments[boids[i]] = i;
            }
        }

        public void ReassignSlots()
        {
            // Simple reassignment based on current list
            var boids = new List<IBoid>(_slotAssignments.Keys);
            AssignSlots(boids);
        }

        public void ReleaseSlot(IBoid boid)
        {
            _slotAssignments.Remove(boid);
        }

        public int? GetSlotIndex(IBoid boid)
        {
            if (_slotAssignments.TryGetValue(boid, out int index)) return index;
            return null;
        }

        public FVector3? GetTargetPosition(IBoid boid)
        {
            if (_currentFormation == null || _leader == null) return null;
            if (!_slotAssignments.TryGetValue(boid, out int index)) return null;

            FVector3 offset = _currentFormation.GetSlotOffset(index, _slotAssignments.Count, _spacing);
            return _currentFormation.GetWorldPosition(offset, _leader.Position, _leader.Forward);
        }
    }
}
