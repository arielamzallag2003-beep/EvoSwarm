using System;
using System.Collections.Generic;

namespace PalaceOfFantasy.FlockForge.Core
{
    public class EventManager : IEventManager
    {
        private readonly Dictionary<Type, Delegate> _handlers = new();

        public void Subscribe<T>(Action<T> handler) where T : struct, IFlockEvent
        {
            var type = typeof(T);
            if (_handlers.TryGetValue(type, out var existing))
                _handlers[type] = Delegate.Combine(existing, handler);
            else
                _handlers[type] = handler;
        }

        public void Unsubscribe<T>(Action<T> handler) where T : struct, IFlockEvent
        {
            var type = typeof(T);
            if (_handlers.TryGetValue(type, out var existing))
            {
                var result = Delegate.Remove(existing, handler);
                if (result == null)
                    _handlers.Remove(type);
                else
                    _handlers[type] = result;
            }
        }

        public void Publish<T>(T eventData) where T : struct, IFlockEvent
        {
            if (_handlers.TryGetValue(typeof(T), out var handler))
                ((Action<T>)handler)?.Invoke(eventData);
        }

        public void Clear() => _handlers.Clear();
    }
}
