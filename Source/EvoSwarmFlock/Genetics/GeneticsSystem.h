#pragma once

#include "CoreMinimal.h"
#include "Genetics/GeneticsTypes.h"
#include "Flock/FlockTypes.h"

// ─────────────────────────────────────────────────────────────────────────────
//  GeneticsSystem.h
//
//  All genetics operations as pure free functions — no virtual dispatch, no
//  heap allocations in the hot path.
//
//  Key functions:
//    MakeDefaultSpeciesConfig  — build the canonical species preset
//    ComputeStatsFromGenome    — genome weights → typed FBoidStats
//    ApplyStatsToSettings      — push stat values into FBoidSettings for the sim
//    MutateGenome              — produce one mutated child genome
//    MateGenomes               — crossover + mutate from two parents
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Build the default FFlockSpeciesConfig with the tuned stat table from the design doc:
 *   TotalStats = 1000, CostToFill and Curve per slot per the agreed table.
 * Call once from UFlockManagerComponent or FFlockSpeciesConfig default constructor.
 */
FFlockSpeciesConfig MakeDefaultSpeciesConfig();

/**
 * Apply a curve formula to the raw fill ratio t ∈ [0, 1].
 *   Linear  → t
 *   Convex  → sqrt(t)   (diminishing returns)
 *   Concave → t*t        (increasing returns)
 */
FORCEINLINE float ApplyCurve(float t, EStatCurve Curve)
{
    switch (Curve)
    {
    case EStatCurve::Convex:  return FMath::Sqrt(FMath::Clamp(t, 0.f, 1.f));
    case EStatCurve::Concave: return FMath::Square(FMath::Clamp(t, 0.f, 1.f));
    default:                  return FMath::Clamp(t, 0.f, 1.f);
    }
}

/**
 * Convert a genome's raw weights into a fully-typed FBoidStats struct.
 * Does NOT modify any mutable fields (Hp, Stamina, Age…) — the caller
 * should copy those from the existing BoidStats entry on first init.
 */
FBoidStats ComputeStatsFromGenome(
    const FFlockGenome&   Genome,
    const FFlockSpeciesConfig& Species);

/**
 * Write the stats that affect movement and perception back into a FBoidSettings
 * so the existing behaviour system uses them correctly:
 *   VitesseMarche  → MaxSpeed
 *   VitesseCourse  → MaxSpeed when bIsSprinting (caller toggles)
 *   Perception     → PerceptionRadius
 */
void ApplyStatsToSettings(
    const FBoidStats& Stats,
    FBoidSettings&    OutSettings,
    bool              bIsSprinting);

/**
 * Produce a mutated child genome from a single parent.
 * Mutation algorithm:
 *   1. Determine N = max(1, round(MutationRate * GStatCount)) swap operations.
 *   2. For each op: pick two random slots i, j.
 *      Transfer delta = FMath::RandRange(1, max(1, Weights[i]/2)) from i to j.
 *   3. Clamp each slot to [MinWeight[slot], MaxWeight[slot]] derived from
 *      CostToFill and TotalStats; redistribute excess to the closest valid slot.
 * Invariant: sum(result.Weights) == Species.TotalStats always.
 */
FFlockGenome MutateGenome(
    const FFlockGenome&    Parent,
    const FFlockSpeciesConfig& Species,
    float                 MutationRate,
    uint32&               RandSeed);

/**
 * Crossover two parent genomes, then mutate the result.
 * Crossover: for each slot, inherit from ParentA or ParentB with 50% chance,
 * then re-normalise to TotalStats, then call MutateGenome.
 */
FFlockGenome MateGenomes(
    const FFlockGenome&    ParentA,
    const FFlockGenome&    ParentB,
    const FFlockSpeciesConfig& Species,
    uint32&               RandSeed);

/**
 * Validate that all weights sum to TotalStats and all are >= 0.
 * Returns false and logs if the invariant is broken (debug / ensure builds).
 */
bool ValidateGenome(const FFlockGenome& Genome, const FFlockSpeciesConfig& Species);
