using System;
using System.Collections.Generic;
using System.Linq;

namespace PalaceOfFantasy.FlockForge.Core
{
    public class Flock : IFlock
    {
        public string Id { get; }
        public string Name { get; set; }
        public bool IsActive { get; set; } = true;
        public IFlockSettings Settings { get; }

        private readonly List<IBoid> _boids = new();
        private readonly BoidContext _contextCache = new();
        private float _totalTime;

        public IReadOnlyList<IBoid> Boids => _boids;
        public int ActiveCount => _boids.Count(b => b.IsActive);

        public event Action<IBoid> OnBoidAdded;
        public event Action<IBoid> OnBoidRemoved;

        public ITargetProvider TargetProvider { get; set; }

        public Flock(string id, IFlockSettings settings)
        {
            Id = id;
            Name = id;
            Settings = settings;
        }

        public void Register(IBoid boid)
        {
            if (!_boids.Contains(boid))
            {
                _boids.Add(boid);
                boid.Flock = this;
                OnBoidAdded?.Invoke(boid);
            }
        }

        public void Unregister(IBoid boid)
        {
            if (_boids.Remove(boid))
            {
                boid.Flock = null;
                OnBoidRemoved?.Invoke(boid);
            }
        }

        public void Clear()
        {
            foreach (var boid in _boids.ToList())
                Unregister(boid);
        }

        public void Step(float deltaTime)
        {
            if (!IsActive) return;

            _totalTime += deltaTime;

            foreach (var boid in _boids)
            {
                if (!boid.IsActive || boid.Settings == null) continue;

                var context = BuildContext(boid, deltaTime);
                FVector3 totalForce = FVector3.Zero;
                float remainingAuthority = 1.0f;

                var allBehaviors = Settings.DefaultBehaviours
                    .Concat(boid.RuntimeBehaviours)
                    .Where(b => b.IsEnabled)
                    .OrderByDescending(b => b.Priority)
                    .ToList();

                foreach (var behaviour in allBehaviors)
                {
                    if (remainingAuthority <= 0.01f) break;

                    var force = behaviour.CalculateForce(context) * behaviour.Weight;
                    float forceMagnitude = force.Magnitude;

                    totalForce += force * remainingAuthority;

                    // If a high-priority behavior is returning a strong force, 
                    // dampen lower-priority behaviors.
                    if (behaviour.Priority > 0 && forceMagnitude > 0.1f)
                    {
                        // Dampen based on priority and force magnitude relative to max speed/force
                        float dampening = (behaviour.Priority / 20.0f) * (forceMagnitude / boid.Settings.MaxSpeed);
                        remainingAuthority = Math.Max(0, remainingAuthority - dampening);
                    }
                }

                boid.AddForce(totalForce);

                boid.ApplyForces(deltaTime);
            }
        }

        private IBoidContext BuildContext(IBoid boid, float deltaTime) 
        {
            _contextCache.Self = boid;
            _contextCache.Flock = this;
            _contextCache.DeltaTime = deltaTime;
            _contextCache.TotalTime = _totalTime;
            _contextCache.Neighbors = GetNeighbors(boid);

            // Use per-boid target provider if available, otherwise use flock-level provider
            var provider = boid.TargetProvider ?? TargetProvider;
            
            if (provider != null)
            {
                _contextCache.SeekTarget = provider.GetSeekTarget(boid);
                _contextCache.SeekTargetBoid = provider.GetSeekTargetBoid(boid);
                _contextCache.Threats = provider.GetThreats(boid);
                _contextCache.NearbyObstacles = provider.GetNearbyObstacles(boid);
            }
            else
            {
                _contextCache.SeekTarget = null;
                _contextCache.SeekTargetBoid = null;
            }

            return _contextCache;
        }

        public IReadOnlyList<IBoid> GetNeighbors(IBoid boid)
        {
            // Guard against null settings
            if (boid.Settings == null)
            {
                return new List<IBoid>();
            }
            
            var radius = boid.Settings.PerceptionRadius;
            var maxNeighbors = boid.Settings.MaxNeighbors;

            return _boids
                .Where(b => b != boid && b.IsActive)
                .Where(b => FVector3.SqrDistance(b.Position, boid.Position) < radius * radius)
                .Take(maxNeighbors)
                .ToList();
        }

        public FVector3 GetCenterOfMass()
        {
            if (_boids.Count == 0) return FVector3.Zero;

            var sum = FVector3.Zero;
            int count = 0;
            foreach (var boid in _boids)
            {
                if (!boid.IsActive) continue;
                sum = sum + boid.Position;
                count++;
            }
            return count > 0 ? sum / count : FVector3.Zero;
        }

        public FVector3 GetAverageVelocity()
        {
            if (_boids.Count == 0) return FVector3.Zero;

            var sum = FVector3.Zero;
            int count = 0;
            foreach (var boid in _boids)
            {
                if (!boid.IsActive) continue;
                sum = sum + boid.Velocity;
                count++;
            }
            return count > 0 ? sum / count : FVector3.Zero;
        }
    }
}
