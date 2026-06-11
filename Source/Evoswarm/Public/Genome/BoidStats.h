// Copyright Evoswarm.
//
// The evolvable genome. EBoidStat enumerates every stat; FBoidGenome is a flat,
// inline, trivially-copyable block of float values (one per stat) so it lives
// cheaply inside a Mass fragment. Cost weights + a per-species budget enforce the
// "Total Stats" invariant: spending on an expensive stat eats more of the budget.

#pragma once

#include "CoreMinimal.h"
#include "BoidStats.generated.h"

class USpeciesConfig;

/** Every evolvable stat. Keep Count last. */
UENUM(BlueprintType)
enum class EBoidStat : uint8
{
	HP                  UMETA(DisplayName = "Hp"),
	Armor               UMETA(DisplayName = "Armure"),
	WalkSpeed           UMETA(DisplayName = "Vitesse Marche"),
	RunSpeed            UMETA(DisplayName = "Vitesse Course"),
	Stamina             UMETA(DisplayName = "Stamina"),
	Regeneration        UMETA(DisplayName = "Regeneration"),
	Hunger              UMETA(DisplayName = "Faim"),
	Biomass             UMETA(DisplayName = "Biomasse"),
	Stealth             UMETA(DisplayName = "Discretion"),
	Damage              UMETA(DisplayName = "Degats"),
	Intimidation        UMETA(DisplayName = "Intimidation"),
	Aggressiveness      UMETA(DisplayName = "Agressivite"),
	Perception          UMETA(DisplayName = "Perception"),
	Diet                UMETA(DisplayName = "Regime Alimentaire"),
	Lifespan            UMETA(DisplayName = "Temps de Vie"),
	ReproductionRate    UMETA(DisplayName = "Taux de Reproduction"),
	Integration         UMETA(DisplayName = "Integration"),
	MutationRate        UMETA(DisplayName = "Taux de Mutation"),

	Count               UMETA(Hidden)
};

/** Number of evolvable stats. */
static constexpr int32 NumBoidStats = static_cast<int32>(EBoidStat::Count);

inline int32 StatIndex(EBoidStat Stat) { return static_cast<int32>(Stat); }

/**
 * The genome: a flat array of stat values. Plain inline data (no UPROPERTY) so it
 * stays trivially copyable and cache-friendly inside a fragment.
 */
USTRUCT()
struct EVOSWARM_API FBoidGenome
{
	GENERATED_BODY()

	float Stats[NumBoidStats] = { 0.f };

	FORCEINLINE float Get(EBoidStat Stat) const { return Stats[StatIndex(Stat)]; }
	FORCEINLINE void  Set(EBoidStat Stat, float Value) { Stats[StatIndex(Stat)] = Value; }
};

/** Per-stat tuning, authored on the species. */
USTRUCT(BlueprintType)
struct EVOSWARM_API FBoidStatDef
{
	GENERATED_BODY()

	/** Lowest allowed value for this stat in this species. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
	float Min = 0.f;

	/** Highest allowed value for this stat in this species. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
	float Max = 1.f;

	/** Budget cost per unit of this stat. >1 makes the stat "expensive". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
	float CostWeight = 1.f;
};

/**
 * Stateless helpers that build, cost and mutate genomes against a species config.
 * All randomness is injected via FRandomStream for determinism.
 */
class EVOSWARM_API FBoidGenomeLibrary
{
public:
	/** Total budget points a genome currently spends, given the species cost weights. */
	static float ComputeCost(const FBoidGenome& Genome, const USpeciesConfig& Species);

	/** Clamp every stat to its species [Min,Max], then scale the above-Min portion so cost <= Budget. */
	static void ClampToBudget(FBoidGenome& Genome, const USpeciesConfig& Species);

	/** Build a fresh genome: every stat at Min, then random budget distributed across stats. */
	static FBoidGenome RandomWithinBudget(const USpeciesConfig& Species, FRandomStream& Rng);

	/** Copy + perturb each stat by +/- MutationRate fraction of its range, then clamp to budget. */
	static FBoidGenome Mutate(const FBoidGenome& Parent, const USpeciesConfig& Species, FRandomStream& Rng);

	/** Sexual reproduction: per-stat pick from one of two parents, then mutate and clamp to budget. */
	static FBoidGenome Crossover(const FBoidGenome& ParentA, const FBoidGenome& ParentB, const USpeciesConfig& Species, FRandomStream& Rng);

	/**
	 * Budget-conserving mutation: shifts points between costed stats at their respective costs
	 * (so the total point budget is preserved), and jitters cost-free stats within their range.
	 * The amount of change scales with MutationRate. This is the per-birth genetic operator.
	 */
	static void Reallocate(FBoidGenome& Genome, const USpeciesConfig& Species, FRandomStream& Rng, float MutationRate);
};
