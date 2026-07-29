using UnityEngine;
using System.Collections.Generic;
using PalaceOfFantasy.FlockForge.Core;
using PalaceOfFantasy.FlockForge.Unity.Config;
using PalaceOfFantasy.FlockForge.Unity.Extensions;

namespace PalaceOfFantasy.FlockForge.Unity.Components
{
    public class BoidAgent : MonoBehaviour, IBoid
    {
        [SerializeField] private BoidProfile _profile;

        private IFlock _flock;
        private FVector3 _velocity;
        private FVector3 _accumulatedForce;
        private static int _nextId = 0;

        public int Id { get; private set; }
        public string Tag { get => this != null ? tag : "Destroyed"; set { if (this != null) tag = value; } }
        public bool IsActive
        {
            get => this != null && gameObject.activeInHierarchy;
            set { if (this != null) gameObject.SetActive(value); }
        }

        public FVector3 Position
        {
            get => this != null ? transform.position.ToFVector3() : FVector3.Zero;
            set { if (this != null) transform.position = value.ToUnityVector3(); }
        }

        public FVector3 Velocity
        {
            get => _velocity;
            set => _velocity = value;
        }

        public FVector3 Forward => this != null ? transform.forward.ToFVector3() : FVector3.Forward;

        public IFlock Flock
        {
            get => _flock;
            set => _flock = value;
        }

        public IBoidSettings Settings => _profile;
        public float SpeedMultiplier { get; set; } = 1f;
        public FVector3 AccumulatedForce => _accumulatedForce;
        
        public IReadOnlyList<IBehaviour> RuntimeBehaviours => _runtimeBehaviours;
        private readonly List<IBehaviour> _runtimeBehaviours = new();

        /// <summary>
        /// Per-boid target provider (e.g., NearestFoodTargetProvider).
        /// Returns null if no provider is attached to this boid.
        /// </summary>
        public ITargetProvider TargetProvider => _targetProvider;
        private ITargetProvider _targetProvider;

        private void Awake()
        {
            Id = _nextId++;
            _targetProvider = GetComponent<ITargetProvider>();
        }

        public void AddRuntimeBehaviour(IBehaviour behaviour)
        {
            if (!_runtimeBehaviours.Contains(behaviour))
                _runtimeBehaviours.Add(behaviour);
        }

        public void RemoveRuntimeBehaviour(IBehaviour behaviour)
        {
            _runtimeBehaviours.Remove(behaviour);
        }

        public void AddForce(FVector3 force)
        {
            _accumulatedForce += force;
        }

        public void ApplyForces(float deltaTime)
        {
            var maxSpeed = Settings.MaxSpeed * SpeedMultiplier;
            var mass = Settings.Mass;
            var drag = Settings.Drag;
            
            // Constrain force based on MovementPlane
            switch (Settings.MovementPlane)
            {
                case MovementPlane.XY:
                    _accumulatedForce = new FVector3(_accumulatedForce.X, _accumulatedForce.Y, 0);
                    break;
                case MovementPlane.XZ:
                    _accumulatedForce = new FVector3(_accumulatedForce.X, 0, _accumulatedForce.Z);
                    break;
            }

            // Apply accumulated force (integration)
            var acceleration = _accumulatedForce / mass;
            _velocity += acceleration * deltaTime;

            // Apply drag
            _velocity *= (1f - drag * deltaTime);

            // Clamp velocity
            _velocity = FVector3.ClampMagnitude(_velocity, maxSpeed);

            // Constrain velocity based on MovementPlane
            switch (Settings.MovementPlane)
            {
                case MovementPlane.XY:
                    _velocity = new FVector3(_velocity.X, _velocity.Y, 0);
                    break;
                case MovementPlane.XZ:
                    _velocity = new FVector3(_velocity.X, 0, _velocity.Z);
                    break;
            }

            // Update position
            transform.position += _velocity.ToUnityVector3() * deltaTime;

            // Update rotation
            if (_velocity.SqrMagnitude > 0.01f)
            {
                transform.forward = _velocity.ToUnityVector3();
            }

            // Reset force
            _accumulatedForce = FVector3.Zero;
        }
    }
}
