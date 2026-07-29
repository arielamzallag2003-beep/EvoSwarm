using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Core
{
    public class State : IState
    {
        public string Name { get; }
        public IReadOnlyList<IBehaviour> Behaviours { get; }

        public State(string name, IEnumerable<IBehaviour> behaviors)
        {
            Name = name;
            Behaviours = new List<IBehaviour>(behaviors);
        }

        public virtual void OnEnter(IBoid boid) { }
        public virtual void OnExit(IBoid boid) { }
        public virtual void OnUpdate(IBoid boid, IBoidContext context, float deltaTime) { }
    }
}
