using UnityEngine;
using PalaceOfFantasy.FlockForge.Core;

namespace PalaceOfFantasy.FlockForge.Unity.Config
{
    [CreateAssetMenu(fileName = "New Boid Profile", menuName = "FlockForge/Boid Profile")]
    public class BoidProfile : ScriptableObject, IBoidSettings
    {
        [Header("Movement")]
        [SerializeField] private float _maxSpeed = 5f;
        [SerializeField] private float _maxForce = 10f;
        [SerializeField] private float _mass = 1f;
        [SerializeField] private float _drag = 0.5f;
        [SerializeField] private MovementPlane _movementPlane = MovementPlane.Free3D;

        [Header("Perception")]
        [SerializeField] private float _perceptionRadius = 5f;
        [SerializeField] private float _fieldOfViewAngle = 270f;
        [SerializeField] private int _maxNeighbors = 10;

        [Header("Avoidance")]
        [SerializeField] private float _obstacleAvoidanceDistance = 3f;

        public float MaxSpeed => _maxSpeed;
        public float MaxForce => _maxForce;
        public float Mass => _mass;
        public float Drag => _drag;
        public MovementPlane MovementPlane => _movementPlane;

        public float PerceptionRadius => _perceptionRadius;
        public float FieldOfViewAngle => _fieldOfViewAngle;
        public int MaxNeighbors => _maxNeighbors;

        public float ObstacleAvoidanceDistance => _obstacleAvoidanceDistance;
    }
}
