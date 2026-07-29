using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IBoid
    {
        int Id { get; }
        string Tag { get; set; }
        bool IsActive { get; set; }

        FVector3 Position { get; set; }
        FVector3 Velocity { get; set; }
        FVector3 Forward { get; }

        IFlock Flock { get; set; }
        IBoidSettings Settings { get; }
        
        /// <summary>
        /// Optional per-boid target provider. If set, takes priority over flock-level provider.
        /// </summary>
        ITargetProvider TargetProvider { get; }
        
        IReadOnlyList<IBehaviour> RuntimeBehaviours { get; }

        void AddForce(FVector3 force);
        void ApplyForces(float deltaTime);
        void AddRuntimeBehaviour(IBehaviour behaviour);
        void RemoveRuntimeBehaviour(IBehaviour behaviour);
        FVector3 AccumulatedForce { get; }
    }
}
