#pragma once

#include "CoreMinimal.h"
#include "GeneticsTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  GeneticsTypes.h
//
//  The core data types for the boid genetics / mini-evolution system.
//
//  Budget model:
//    - Each FFlockGenome stores 18 integer weights that always sum to TotalStats.
//    - FStatConfig per slot defines Min/Max range, CostToFill, and Curve.
//    - ComputeStatsFromGenome() in GeneticsSystem.cpp converts weights → typed
//      FBoidStats using:  t = ApplyCurve( weights[i] / CostToFill[i] )
//                         value = Lerp(Min, Max, t)
//    - MutateGenome() and MateGenomes() shuffle budget between slots while
//      preserving the invariant: sum(Weights) == TotalStats.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
//  EStatSlot — index enum for the 18 genome slots
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EStatSlot : uint8
{
    Hp                  = 0,
    Armure              = 1,
    VitesseMarche       = 2,
    VitesseCourse       = 3,
    Stamina             = 4,
    Regeneration        = 5,
    Faim                = 6,
    Biomasse            = 7,
    Discretion          = 8,
    Degats              = 9,
    Intimidation        = 10,
    Agressivite         = 11,
    Perception          = 12,
    RegimeAlimentaire   = 13,
    TempsDeVie          = 14,
    TauxDeReproduction  = 15,
    Integration         = 16,
    TauxDeMutation      = 17,

    Count               UMETA(Hidden),
};

static constexpr int32 GStatCount = static_cast<int32>(EStatSlot::Count);

// ─────────────────────────────────────────────────────────────────────────────
//  EStatCurve — how budget points convert to fill ratio
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EStatCurve : uint8
{
    /** t = w / C         — every point equally valuable           */
    Linear  UMETA(DisplayName = "Linear"),

    /** t = sqrt(w / C)  — first points cheap, later very expensive */
    Convex  UMETA(DisplayName = "Convex (Diminishing Returns)"),

    /** t = (w / C)^2    — only pays off with heavy investment      */
    Concave UMETA(DisplayName = "Concave (Increasing Returns)"),
};

// ─────────────────────────────────────────────────────────────────────────────
//  FStatConfig — species-level config for a single genome slot
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FStatConfig
{
    GENERATED_BODY()

    /** Minimum actual value when weight = 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float Min = 0.f;

    /** Maximum actual value when weight >= CostToFill. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float Max = 1.f;

    /**
     * Budget points needed to go from Min to Max.
     * See implementation_plan for the per-stat table (total = 2250 vs budget 1000).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat", meta = (ClampMin = "1"))
    int32 CostToFill = 100;

    /** Scaling curve — controls marginal per-point cost. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    EStatCurve Curve = EStatCurve::Linear;

    /** Human-readable name for editor / debug display. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    FString DisplayName;
};

// ─────────────────────────────────────────────────────────────────────────────
//  FFlockGenome — the genetic data of one boid
//  Invariant: sum(Weights[i]) == Species.TotalStats  (always)
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FFlockGenome
{
    GENERATED_BODY()

    // UE 5.7: C-style static arrays cannot be Blueprint-exposed.
    UPROPERTY(EditAnywhere, Category = "Genome")
    int32 Weights[GStatCount] = {};

    /** Convenience: sum all weights (should equal TotalStats). */
    int32 TotalWeight() const
    {
        int32 Sum = 0;
        for (int32 i = 0; i < GStatCount; ++i) Sum += Weights[i];
        return Sum;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  FBoidStats — fully-typed runtime stat values computed from a genome.
//  Stored per-boid in FFlock::BoidStats (parallel to Boids array).
//  Current mutable values (Hp, Stamina, etc.) live alongside their maxima.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FBoidStats
{
    GENERATED_BODY()

    // ── Combat / Survival ────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Stats") int32 MaxHp       = 100;
    UPROPERTY(BlueprintReadOnly,  Category = "Stats") int32 Hp          = 100;  // current

    UPROPERTY(BlueprintReadWrite, Category = "Stats") int32 Armure      = 0;

    // ── Movement ─────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Stats") float VitesseMarche  = 5.f;
    UPROPERTY(BlueprintReadWrite, Category = "Stats") float VitesseCourse  = 12.f;

    UPROPERTY(BlueprintReadWrite, Category = "Stats") float MaxStamina  = 50.f;
    UPROPERTY(BlueprintReadOnly,  Category = "Stats") float Stamina     = 50.f; // current

    UPROPERTY(BlueprintReadWrite, Category = "Stats") float Regeneration = 0.f; // HP/s

    // ── Ecology ──────────────────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Stats") int32 MaxFaim     = 100;
    UPROPERTY(BlueprintReadOnly,  Category = "Stats") int32 Faim        = 0;    // current food stored

    UPROPERTY(BlueprintReadWrite, Category = "Stats") int32 Biomasse    = 20;

    // ── Behaviour multipliers ─────────────────────────────────────────────────
    UPROPERTY(BlueprintReadWrite, Category = "Stats") float Discretion      = 0.f; // [0,1]
    UPROPERTY(BlueprintReadWrite, Category = "Stats") int32 Degats          = 0;
    UPROPERTY(BlueprintReadWrite, Category = "Stats") float Intimidation    = 0.f; // [0,1]
    UPROPERTY(BlueprintReadWrite, Category = "Stats") float Agressivite     = 0.f; // [0,1]
    UPROPERTY(BlueprintReadWrite, Category = "Stats") float Perception      = 10.f;

    // ── Identity / Lifecycle ─────────────────────────────────────────────────
    /** 0 = Herbivore, 0.5 = Omnivore, 1 = Carnivore */
    UPROPERTY(BlueprintReadWrite, Category = "Stats") float RegimeAlimentaire  = 0.5f;
    UPROPERTY(BlueprintReadWrite, Category = "Stats") float TempsDeVie         = 120.f; // in sim-seconds
    UPROPERTY(BlueprintReadOnly,  Category = "Stats") float Age                = 0.f;   // current

    UPROPERTY(BlueprintReadWrite, Category = "Stats") float TauxDeReproduction = 0.1f;
    UPROPERTY(BlueprintReadOnly,  Category = "Stats") float ReproductionAccum  = 0.f;  // timer

    UPROPERTY(BlueprintReadWrite, Category = "Stats") float Integration        = 0.5f;
    UPROPERTY(BlueprintReadWrite, Category = "Stats") float TauxDeMutation     = 0.05f;
};

// ─────────────────────────────────────────────────────────────────────────────
//  FFlockSpeciesConfig — shared species-level settings, stored once in FFlock.
//  Defines the stat ranges and budget that all boids in the flock share.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FFlockSpeciesConfig
{
    GENERATED_BODY()

    /** Fixed budget. sum(Genome.Weights) must always equal this. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species", meta = (ClampMin = "18"))
    int32 TotalStats = 1000;

    /**
     * Per-slot config: ranges, CostToFill, and curve.
     * Defaults are set in GeneticsSystem::MakeDefaultSpeciesConfig().
     */
    // UE 5.7: C-style static arrays cannot be Blueprint-exposed.
    UPROPERTY(EditAnywhere, Category = "Species")
    FStatConfig StatConfigs[GStatCount];

    /**
     * Default genome — equal distribution at species creation.
     * Mutated copies of this seed the initial population.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species")
    FFlockGenome DefaultGenome;

    /** Stamina drain rate per second while sprinting (not a genome stat). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species", meta = (ClampMin = "0"))
    float StaminaDrainRate = 20.f;

    /** Stamina regen rate per second while NOT sprinting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species", meta = (ClampMin = "0"))
    float StaminaRegenRate = 10.f;

    /** Speed above this fraction of MaxSpeed triggers sprinting (0–1). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Species", meta = (ClampMin = "0", ClampMax = "1"))
    float SprintThreshold = 0.7f;
};
