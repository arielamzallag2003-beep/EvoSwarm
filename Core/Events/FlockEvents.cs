namespace PalaceOfFantasy.FlockForge.Core
{
    public readonly struct StateChangedEvent : IFlockEvent
    {
        public readonly IBoid Boid;
        public readonly IState OldState;
        public readonly IState NewState;

        public StateChangedEvent(IBoid boid, IState oldState, IState newState)
        {
            Boid = boid;
            OldState = oldState;
            NewState = newState;
        }
    }

    public readonly struct ThreatDetectedEvent : IFlockEvent
    {
        public readonly IBoid Boid;
        public readonly FVector3 ThreatPosition;

        public ThreatDetectedEvent(IBoid boid, FVector3 threatPosition)
        {
            Boid = boid;
            ThreatPosition = threatPosition;
        }
    }

    public readonly struct TargetReachedEvent : IFlockEvent
    {
        public readonly IBoid Boid;
        public readonly FVector3 Target;

        public TargetReachedEvent(IBoid boid, FVector3 target)
        {
            Boid = boid;
            Target = target;
        }
    }

    public readonly struct BoidCapturedEvent : IFlockEvent
    {
        public readonly IBoid Prey;
        public readonly IBoid Predator;

        public BoidCapturedEvent(IBoid prey, IBoid predator)
        {
            Prey = prey;
            Predator = predator;
        }
    }
}
