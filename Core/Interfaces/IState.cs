using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IState
    {
        string Name { get; }
        IReadOnlyList<IBehaviour> Behaviours { get; }

        void OnEnter(IBoid boid);
        void OnExit(IBoid boid);
        void OnUpdate(IBoid boid, IBoidContext context, float deltaTime);
    }
}
