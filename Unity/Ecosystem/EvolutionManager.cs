using UnityEngine;
using System.Collections.Generic;
using System.Linq;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Manages the evolution of boid populations through genetic algorithms.
    /// Tracks fitness of all boids, handles breeding, and maintains gene pools.
    /// </summary>
    public class EvolutionManager : MonoBehaviour
    {
        public static EvolutionManager Instance { get; private set; }

        [Header("Evolution Settings")]
        [SerializeField] private float _mutationRate = 0.1f;
        [SerializeField] private float _mutationStrength = 0.15f;
        [SerializeField] private int _eliteCount = 3;
        [SerializeField] private int _tournamentSize = 3;
        
        [Header("Statistics")]
        [SerializeField] private int _preyGeneration = 0;
        [SerializeField] private int _predatorGeneration = 0;
        [SerializeField] private float _avgPreySurvival = 0f;
        [SerializeField] private float _avgPredatorKills = 0f;
        [SerializeField] private int _totalPreyDeaths = 0;
        [SerializeField] private int _totalPredatorDeaths = 0;

        // History for visualization (last 20 generations)
        private List<float> _preySurvivalHistory = new List<float>();
        private List<float> _predatorKillHistory = new List<float>();
        private float _bestPreySurvival = 0f;
        private float _bestPredatorKills = 0f;
        private float _startTime;

        // Gene pools - store genes of deceased boids for breeding
        private List<GeneRecord> _preyGenePool = new List<GeneRecord>();
        private List<GeneRecord> _predatorGenePool = new List<GeneRecord>();
        
        // Elite genes - best performers
        private List<GeneRecord> _preyElite = new List<GeneRecord>();
        private List<GeneRecord> _predatorElite = new List<GeneRecord>();

        // Public stats
        public int PreyGeneration => _preyGeneration;
        public int PredatorGeneration => _predatorGeneration;
        public float AvgPreySurvival => _avgPreySurvival;
        public float AvgPredatorKills => _avgPredatorKills;
        public float BestPreySurvival => _bestPreySurvival;
        public float BestPredatorKills => _bestPredatorKills;
        public List<float> PreySurvivalHistory => _preySurvivalHistory;
        public List<float> PredatorKillHistory => _predatorKillHistory;
        public float ElapsedTime => Time.time - _startTime;

        private struct GeneRecord
        {
            public float[] Genes;
            public float Fitness;
            
            public GeneRecord(float[] genes, float fitness)
            {
                Genes = (float[])genes.Clone();
                Fitness = fitness;
            }
        }

        private void Awake()
        {
            if (Instance != null && Instance != this)
            {
                Destroy(gameObject);
                return;
            }
            Instance = this;
            _startTime = Time.time;
        }

        private void OnDestroy()
        {
            if (Instance == this) Instance = null;
        }

        /// <summary>
        /// Called when a prey dies. Records its genes and fitness.
        /// </summary>
        public void RecordPreyDeath(BoidBrain brain)
        {
            if (brain == null) return;
            
            float survivalTime = brain.SurvivalTime;
            float fitness = survivalTime + brain.FoodEaten * 2f;
            var record = new GeneRecord(brain.Genes, fitness);
            
            _preyGenePool.Add(record);
            _totalPreyDeaths++;
            
            // Track best
            if (survivalTime > _bestPreySurvival)
            {
                _bestPreySurvival = survivalTime;
            }
            
            // Update elite pool
            UpdateElitePool(_preyElite, record);
            
            // Update stats
            UpdatePreyStats();
            
            // Check for generation advancement
            if (_totalPreyDeaths % 20 == 0)
            {
                _preyGeneration++;
                TrimGenePool(_preyGenePool, 50);
                
                // Record history
                _preySurvivalHistory.Add(_avgPreySurvival);
                if (_preySurvivalHistory.Count > 20) _preySurvivalHistory.RemoveAt(0);
                
                UnityEngine.Debug.Log($"[Evolution] Prey Gen {_preyGeneration} - Avg: {_avgPreySurvival:F1}s, Best: {_bestPreySurvival:F1}s");
            }
        }

        /// <summary>
        /// Called when a predator dies. Records its genes and fitness.
        /// </summary>
        public void RecordPredatorDeath(BoidBrain brain)
        {
            if (brain == null) return;
            
            int kills = brain.KillCount;
            float fitness = kills * 10f + brain.SurvivalTime * 0.5f;
            var record = new GeneRecord(brain.Genes, fitness);
            
            _predatorGenePool.Add(record);
            _totalPredatorDeaths++;
            
            // Track best
            if (kills > _bestPredatorKills)
            {
                _bestPredatorKills = kills;
            }
            
            // Update elite pool
            UpdateElitePool(_predatorElite, record);
            
            // Update stats
            UpdatePredatorStats();
            
            // Check for generation advancement
            if (_totalPredatorDeaths % 10 == 0)
            {
                _predatorGeneration++;
                TrimGenePool(_predatorGenePool, 30);
                
                // Record history
                _predatorKillHistory.Add(_avgPredatorKills);
                if (_predatorKillHistory.Count > 20) _predatorKillHistory.RemoveAt(0);
                
                UnityEngine.Debug.Log($"[Evolution] Predator Gen {_predatorGeneration} - Avg: {_avgPredatorKills:F1}, Best: {_bestPredatorKills:F0} kills");
            }
        }

        /// <summary>
        /// Get evolved genes for a new prey
        /// </summary>
        public float[] GetPreyGenes()
        {
            return GetEvolvedGenes(_preyGenePool, _preyElite);
        }

        /// <summary>
        /// Get evolved genes for a new predator
        /// </summary>
        public float[] GetPredatorGenes()
        {
            return GetEvolvedGenes(_predatorGenePool, _predatorElite);
        }

        private float[] GetEvolvedGenes(List<GeneRecord> pool, List<GeneRecord> elite)
        {
            float[] genes = new float[BoidBrain.TOTAL_GENES];
            
            // If not enough records, create random genes
            if (pool.Count < 3 && elite.Count < 1)
            {
                for (int i = 0; i < genes.Length; i++)
                {
                    genes[i] = Random.value;
                }
                return genes;
            }

            // Select parents via tournament selection
            float[] parent1 = TournamentSelect(pool, elite);
            float[] parent2 = TournamentSelect(pool, elite);
            
            // Crossover and mutation
            for (int i = 0; i < genes.Length; i++)
            {
                // Crossover
                genes[i] = Random.value > 0.5f ? parent1[i] : parent2[i];
                
                // Mutation
                if (Random.value < _mutationRate)
                {
                    float mutation = (Random.value - 0.5f) * 2f * _mutationStrength;
                    genes[i] = Mathf.Clamp01(genes[i] + mutation);
                }
            }
            
            return genes;
        }

        private float[] TournamentSelect(List<GeneRecord> pool, List<GeneRecord> elite)
        {
            // Combine pool and elite, preferring elite
            var combined = new List<GeneRecord>();
            combined.AddRange(elite);
            
            // Add random samples from pool
            for (int i = 0; i < _tournamentSize && pool.Count > 0; i++)
            {
                combined.Add(pool[Random.Range(0, pool.Count)]);
            }
            
            if (combined.Count == 0)
            {
                // Fallback: random genes
                float[] random = new float[BoidBrain.TOTAL_GENES];
                for (int i = 0; i < random.Length; i++) random[i] = Random.value;
                return random;
            }
            
            // Select best from tournament
            return combined.OrderByDescending(r => r.Fitness).First().Genes;
        }

        private void UpdateElitePool(List<GeneRecord> elite, GeneRecord newRecord)
        {
            elite.Add(newRecord);
            
            // Keep only top performers
            if (elite.Count > _eliteCount)
            {
                elite.Sort((a, b) => b.Fitness.CompareTo(a.Fitness));
                elite.RemoveRange(_eliteCount, elite.Count - _eliteCount);
            }
        }

        private void TrimGenePool(List<GeneRecord> pool, int maxSize)
        {
            if (pool.Count > maxSize)
            {
                // Keep top half by fitness
                pool.Sort((a, b) => b.Fitness.CompareTo(a.Fitness));
                pool.RemoveRange(maxSize, pool.Count - maxSize);
            }
        }

        private void UpdatePreyStats()
        {
            if (_preyGenePool.Count == 0) return;
            
            float totalSurvival = 0f;
            int recent = Mathf.Min(20, _preyGenePool.Count);
            for (int i = _preyGenePool.Count - recent; i < _preyGenePool.Count; i++)
            {
                totalSurvival += _preyGenePool[i].Fitness;
            }
            _avgPreySurvival = totalSurvival / recent;
        }

        private void UpdatePredatorStats()
        {
            if (_predatorGenePool.Count == 0) return;
            
            float totalKills = 0f;
            int recent = Mathf.Min(10, _predatorGenePool.Count);
            for (int i = _predatorGenePool.Count - recent; i < _predatorGenePool.Count; i++)
            {
                totalKills += _predatorGenePool[i].Fitness / 10f; // Convert back from fitness
            }
            _avgPredatorKills = totalKills / recent;
        }

        /// <summary>
        /// Get evolution statistics for UI display
        /// </summary>
        public string GetStatsString()
        {
            return $"Gen P:{_preyGeneration} H:{_predatorGeneration} | " +
                   $"Surv:{_avgPreySurvival:F0}s Kill:{_avgPredatorKills:F1}";
        }
    }
}
