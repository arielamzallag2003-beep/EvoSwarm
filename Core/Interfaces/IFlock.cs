using System;
using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IFlock
    {
        string Id { get; }
        string Name { get; set; }
        bool IsActive { get; set; }

        IReadOnlyList<IBoid> Boids { get; }
        int ActiveCount { get; }
        IFlockSettings Settings { get; }

        void Register(IBoid boid);
        void Unregister(IBoid boid);
        void Clear();

        void Step(float deltaTime);

        IReadOnlyList<IBoid> GetNeighbors(IBoid boid);
        FVector3 GetCenterOfMass();
        FVector3 GetAverageVelocity();

        event Action<IBoid> OnBoidAdded;
        event Action<IBoid> OnBoidRemoved;
    }
}
