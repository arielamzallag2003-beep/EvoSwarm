using System;

namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IEventManager
    {
        void Subscribe<T>(Action<T> handler) where T : struct, IFlockEvent;
        void Unsubscribe<T>(Action<T> handler) where T : struct, IFlockEvent;
        void Publish<T>(T eventData) where T : struct, IFlockEvent;
        void Clear();
    }

    public interface IFlockEvent { }
}
