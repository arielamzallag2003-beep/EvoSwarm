using UnityEngine;
using System.Collections.Generic;
using PalaceOfFantasy.FlockForge.Unity.Components;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Central manager for the prey-predator ecosystem.
    /// Handles spawning, population limits, and death/rebirth cycles.
    /// </summary>
    public class EcosystemManager : MonoBehaviour
    {
        [Header("Prefabs")]
        [SerializeField] private GameObject _preyPrefab;
        [SerializeField] private GameObject _predatorPrefab;
        [SerializeField] private GameObject _foodPrefab;

        [Header("Flock Managers")]
        [SerializeField] private FlockManager _preyFlockManager;
        [SerializeField] private FlockManager _predatorFlockManager;

        [Header("Population Limits")]
        [SerializeField] private int _maxPrey = 100;
        [SerializeField] private int _maxPredators = 20;
        [SerializeField] private int _maxFood = 10;

        [Header("Initial Spawn")]
        [SerializeField] private int _initialPrey = 30;
        [SerializeField] private int _initialPredators = 5;
        [SerializeField] private int _initialFood = 5;

        [Header("Spawn Settings")]
        [SerializeField] private float _spawnRadius = 25f;
        [SerializeField] private float _minHeight = 5f;
        [SerializeField] private float _maxHeight = 40f;

        [Header("Predator Settings")]
        [SerializeField] private float _predatorLifespan = 30f;
        [SerializeField, Range(0, 1)] private float _predatorReproductionChance = 0.2f; // Reduced from 0.6
        [SerializeField, Range(0, 1)] private float _preyReproductionChance = 0.5f; // Chance per bite

        // Tracking
        private List<GameObject> _allPrey = new List<GameObject>();
        private List<GameObject> _allPredators = new List<GameObject>();
        private List<GameObject> _allFood = new List<GameObject>();

        // Singleton
        public static EcosystemManager Instance { get; private set; }

        // Properties for UI
        public int PreyCount => _allPrey.Count;
        public int PredatorCount => _allPredators.Count;
        public int FoodCount => _allFood.Count;
        public int MaxPrey => _maxPrey;
        public int MaxPredators => _maxPredators;

        private void Awake()
        {
            if (Instance != null && Instance != this)
            {
                Destroy(gameObject);
                return;
            }
            Instance = this;
        }

        private void Start()
        {
            SpawnInitialPopulation();
        }

        private void SpawnInitialPopulation()
        {
            // Spawn initial food
            for (int i = 0; i < _initialFood; i++)
            {
                SpawnFood();
            }

            // Spawn initial prey
            for (int i = 0; i < _initialPrey; i++)
            {
                SpawnPrey();
            }

            // Spawn initial predators
            for (int i = 0; i < _initialPredators; i++)
            {
                SpawnPredator();
            }
        }

        public GameObject SpawnPrey()
        {
            if (_allPrey.Count >= _maxPrey || _preyPrefab == null) return null;

            Vector3 pos = GetRandomSpawnPosition();
            var prey = Instantiate(_preyPrefab, pos, Quaternion.identity);
            prey.name = $"Prey_{_allPrey.Count}";
            
            // Add catchable component if not present
            if (prey.GetComponent<PreyCatchable>() == null)
            {
                prey.AddComponent<PreyCatchable>();
            }

            // Add Hunger component
            if (prey.GetComponent<PreyHunger>() == null)
            {
                prey.AddComponent<PreyHunger>();
            }

            // Add and initialize BoidBrain with evolved genes
            var brain = prey.GetComponent<BoidBrain>();
            if (brain == null)
            {
                brain = prey.AddComponent<BoidBrain>();
            }
            if (EvolutionManager.Instance != null)
            {
                brain.SetGenes(EvolutionManager.Instance.GetPreyGenes());
            }

            // Register with FlockManager
            var boidAgent = prey.GetComponent<BoidAgent>();
            if (boidAgent != null && _preyFlockManager != null)
            {
                _preyFlockManager.RegisterBoid(boidAgent);
            }

            // Add Gene Applicator (ensure it's present for food logic)
            if (prey.GetComponent<BoidGeneApplicator>() == null)
            {
                var applicator = prey.AddComponent<BoidGeneApplicator>();
                // In a real scenario, we'd need to set _isPrey via reflection if internal
            }

            _allPrey.Add(prey);
            return prey;
        }

        public GameObject SpawnPredator()
        {
            if (_allPredators.Count >= _maxPredators || _predatorPrefab == null) return null;

            Vector3 pos = GetRandomSpawnPosition();
            var predator = Instantiate(_predatorPrefab, pos, Quaternion.identity);
            predator.name = $"Predator_{_allPredators.Count}";
            
            // Add lifespan component if not present
            var lifespan = predator.GetComponent<PredatorLifespan>();
            if (lifespan == null)
            {
                lifespan = predator.AddComponent<PredatorLifespan>();
            }
            lifespan.Initialize(_predatorLifespan);

            // Add and initialize BoidBrain with evolved genes
            var brain = predator.GetComponent<BoidBrain>();
            if (brain == null)
            {
                brain = predator.AddComponent<BoidBrain>();
            }
            if (EvolutionManager.Instance != null)
            {
                brain.SetGenes(EvolutionManager.Instance.GetPredatorGenes());
            }

            // Register with FlockManager
            var boidAgent = predator.GetComponent<BoidAgent>();
            if (boidAgent != null && _predatorFlockManager != null)
            {
                _predatorFlockManager.RegisterBoid(boidAgent);
            }

            _allPredators.Add(predator);
            return predator;
        }

        public GameObject SpawnFood()
        {
            if (_allFood.Count >= _maxFood || _foodPrefab == null) return null;

            // 50% chance to spawn in a new patch if we are low on food
            if (_allFood.Count % 5 == 0) 
            {
                 return SpawnFoodPatch(GetRandomSpawnPosition(), 5);
            }

            return InternalSpawnFood(GetRandomSpawnPosition());
        }

        private GameObject SpawnFoodPatch(Vector3 center, int count)
        {
            GameObject first = null;
            for (int i = 0; i < count; i++)
            {
                if (_allFood.Count >= _maxFood) break;
                Vector3 offset = Random.insideUnitSphere * 5f;
                var food = InternalSpawnFood(center + offset);
                if (first == null) first = food;
            }
            return first;
        }

        private GameObject InternalSpawnFood(Vector3 pos)
        {
            var food = Instantiate(_foodPrefab, pos, Quaternion.identity);
            food.name = $"Food_{_allFood.Count}";
            
            if (food.GetComponent<FoodTarget>() == null)
            {
                food.AddComponent<FoodTarget>();
            }

            _allFood.Add(food);
            return food;
        }

        public void OnPreyEaten(GameObject prey, GameObject predator)
        {
            if (prey != null && _allPrey.Contains(prey))
            {
                _allPrey.Remove(prey);
                
                // Report to evolution manager
                var preyBrain = prey.GetComponent<BoidBrain>();
                if (preyBrain != null && EvolutionManager.Instance != null)
                {
                    EvolutionManager.Instance.RecordPreyDeath(preyBrain);
                }
                
                // Record kill for predator
                var predatorBrain = predator?.GetComponent<BoidBrain>();
                if (predatorBrain != null)
                {
                    predatorBrain.RecordKill();
                }
                
                // Unregister from flock
                var boidAgent = prey.GetComponent<BoidAgent>();
                if (boidAgent != null && _preyFlockManager != null)
                {
                    _preyFlockManager.UnregisterBoid(boidAgent);
                }
                
                Destroy(prey);

                // Predator breeds when eating (chance-based)
                if (Random.value < _predatorReproductionChance)
                {
                    SpawnPredator();
                }

                // Reset predator lifespan
                var lifespan = predator?.GetComponent<PredatorLifespan>();
                if (lifespan != null)
                {
                    lifespan.ResetLifespan();
                }
            }
        }

        public void OnFoodBite(GameObject prey)
        {
            if (prey == null) return;

            // Record food eaten for prey's fitness
            var preyBrain = prey.GetComponent<BoidBrain>();
            if (preyBrain != null)
            {
                preyBrain.RecordFoodEaten();
            }

            // Spawn new prey (chance-based on every bite)
            if (Random.value < _preyReproductionChance)
            {
                SpawnPrey();
            }
        }

        public void OnFoodEaten(GameObject food, GameObject prey = null)
        {
            if (food != null && _allFood.Contains(food))
            {
                _allFood.Remove(food);
                
                // Respawn food at new location with delay
                StartCoroutine(RespawnFoodWithDelay(5.0f));
                
                Destroy(food);
            }
        }

        private System.Collections.IEnumerator RespawnFoodWithDelay(float delay)
        {
            yield return new WaitForSeconds(delay);
            SpawnFood();
        }

        public void OnPredatorDied(GameObject predator)
        {
            if (predator != null && _allPredators.Contains(predator))
            {
                _allPredators.Remove(predator);
                
                // Report to evolution manager
                var brain = predator.GetComponent<BoidBrain>();
                if (brain != null && EvolutionManager.Instance != null)
                {
                    EvolutionManager.Instance.RecordPredatorDeath(brain);
                }
                
                // Unregister from flock
                var boidAgent = predator.GetComponent<BoidAgent>();
                if (boidAgent != null && _predatorFlockManager != null)
                {
                    _predatorFlockManager.UnregisterBoid(boidAgent);
                }
                
                Destroy(predator);
            }
        }

        public GameObject GetNearestPrey(Vector3 position)
        {
            GameObject nearest = null;
            float nearestDist = float.MaxValue;

            foreach (var prey in _allPrey)
            {
                if (prey == null) continue;
                float dist = Vector3.Distance(position, prey.transform.position);
                if (dist < nearestDist)
                {
                    nearestDist = dist;
                    nearest = prey;
                }
            }

            return nearest;
        }

        public GameObject GetNearestFood(Vector3 position)
        {
            GameObject nearest = null;
            float nearestDist = float.MaxValue;

            foreach (var food in _allFood)
            {
                if (food == null) continue;
                float dist = Vector3.Distance(position, food.transform.position);
                if (dist < nearestDist)
                {
                    nearestDist = dist;
                    nearest = food;
                }
            }

            return nearest;
        }

        private Vector3 GetRandomSpawnPosition()
        {
            // Full 3D spawn within sphere, clamped to vertical bounds
            Vector3 randomSphere = Random.insideUnitSphere * _spawnRadius;
            float y = Mathf.Clamp(randomSphere.y + (_minHeight + _maxHeight) * 0.5f, _minHeight, _maxHeight);
            return new Vector3(randomSphere.x, y, randomSphere.z);
        }

        // Cleanup on destroy
        private void OnDestroy()
        {
            if (Instance == this) Instance = null;
        }
    }
}
