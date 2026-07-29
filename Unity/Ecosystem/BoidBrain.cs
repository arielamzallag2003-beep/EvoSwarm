using UnityEngine;
using System;

namespace PalaceOfFantasy.FlockForge.Unity.Ecosystem
{
    /// <summary>
    /// Holds evolvable genes for a boid. These genes affect behavior weights,
    /// perception, and other parameters. Genes are inherited from parents
    /// with mutation applied for evolution.
    /// </summary>
    public class BoidBrain : MonoBehaviour
    {
        [Header("Gene Values (0-1 normalized)")]
        [SerializeField] private float[] _genes;
        
        [Header("Fitness Tracking")]
        [SerializeField] private float _spawnTime;
        [SerializeField] private int _killCount;
        [SerializeField] private int _foodEaten;
        
        // Gene indices for prey
        public const int GENE_FLEE_WEIGHT = 0;
        public const int GENE_SEPARATION_WEIGHT = 1;
        public const int GENE_COHESION_WEIGHT = 2;
        public const int GENE_PANIC_DISTANCE = 3;
        public const int GENE_SPEED_MODIFIER = 4;
        public const int GENE_PERCEPTION_RADIUS = 5;
        
        // Gene indices for predators (reuse some)
        public const int GENE_SEEK_WEIGHT = 0;
        public const int GENE_PERSISTENCE = 6;
        public const int GENE_AGGRESSION = 7;
        
        public const int TOTAL_GENES = 8;
        
        public float[] Genes => _genes;
        public float SpawnTime => _spawnTime;
        public int KillCount => _killCount;
        public int FoodEaten => _foodEaten;
        
        /// <summary>
        /// Survival time in seconds
        /// </summary>
        public float SurvivalTime => Time.time - _spawnTime;
        
        /// <summary>
        /// Fitness score for this boid
        /// </summary>
        public float Fitness
        {
            get
            {
                // For prey: survival time + food bonus
                // For predators: kills * 10 + survival bonus
                float survivalFitness = SurvivalTime;
                float killFitness = _killCount * 10f;
                float foodFitness = _foodEaten * 2f;
                return survivalFitness + killFitness + foodFitness;
            }
        }

        private void Awake()
        {
            if (_genes == null || _genes.Length != TOTAL_GENES)
            {
                _genes = new float[TOTAL_GENES];
                RandomizeGenes();
            }
            _spawnTime = Time.time;
        }

        /// <summary>
        /// Initialize with random genes
        /// </summary>
        public void RandomizeGenes()
        {
            for (int i = 0; i < TOTAL_GENES; i++)
            {
                _genes[i] = UnityEngine.Random.value;
            }
        }

        /// <summary>
        /// Initialize with genes from parents + mutation
        /// </summary>
        public void InheritGenes(float[] parent1Genes, float[] parent2Genes, float mutationRate = 0.1f, float mutationStrength = 0.15f)
        {
            if (_genes == null || _genes.Length != TOTAL_GENES)
            {
                _genes = new float[TOTAL_GENES];
            }
            
            for (int i = 0; i < TOTAL_GENES; i++)
            {
                // Crossover: randomly pick from either parent
                _genes[i] = UnityEngine.Random.value > 0.5f ? parent1Genes[i] : parent2Genes[i];
                
                // Mutation
                if (UnityEngine.Random.value < mutationRate)
                {
                    float mutation = (UnityEngine.Random.value - 0.5f) * 2f * mutationStrength;
                    _genes[i] = Mathf.Clamp01(_genes[i] + mutation);
                }
            }
            
            _spawnTime = Time.time;
            _killCount = 0;
            _foodEaten = 0;
        }

        /// <summary>
        /// Set genes directly (for cloning elite)
        /// </summary>
        public void SetGenes(float[] genes)
        {
            if (_genes == null || _genes.Length != TOTAL_GENES)
            {
                _genes = new float[TOTAL_GENES];
            }
            Array.Copy(genes, _genes, TOTAL_GENES);
            _spawnTime = Time.time;
            _killCount = 0;
            _foodEaten = 0;
        }

        public void RecordKill() => _killCount++;
        public void RecordFoodEaten() => _foodEaten++;

        #region Gene Accessors with Range Mapping
        
        /// <summary>
        /// Map a gene (0-1) to a specific range
        /// </summary>
        private float MapGene(int index, float min, float max)
        {
            return Mathf.Lerp(min, max, _genes[index]);
        }

        // Prey genes
        public float FleeWeight => MapGene(GENE_FLEE_WEIGHT, 0.2f, 5.0f);
        public float SeparationWeight => MapGene(GENE_SEPARATION_WEIGHT, 0.1f, 4.0f);
        public float CohesionWeight => MapGene(GENE_COHESION_WEIGHT, 0.1f, 3.0f);
        public float PanicDistance => MapGene(GENE_PANIC_DISTANCE, 2f, 30f);
        public float SpeedModifier => MapGene(GENE_SPEED_MODIFIER, 0.5f, 2.0f);
        public float PerceptionRadius => MapGene(GENE_PERCEPTION_RADIUS, 5f, 40f);
        
        // Predator genes
        public float SeekWeight => MapGene(GENE_SEEK_WEIGHT, 0.2f, 5.0f);
        public float Persistence => MapGene(GENE_PERSISTENCE, 1f, 15f);
        public float Aggression => MapGene(GENE_AGGRESSION, 0.2f, 3.0f);
        
        #endregion
        
        /// <summary>
        /// Get a color representing this brain's genes (for visualization)
        /// </summary>
        public Color GetGeneColor()
        {
            // Use first 3 genes as RGB
            return new Color(_genes[0], _genes[1], _genes[2], 1f);
        }
    }
}
