#pragma once

#include "CoreMinimal.h"
#include "Flock/FlockTypes.h"
#include "Flock/SpatialGrid.h"
#include "Behaviours/BehaviourTypes.h"
#include "StateMachine/StateMachineTypes.h"
#include "Formations/FormationTypes.h"
#include "Events/FlockEvents.h"
#include "Genetics/GeneticsTypes.h"
#include "Genetics/GeneticsSystem.h"
#include "FlockContainer.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FFlock
//
//  Replaces: class Flock : IFlock  (heap object)
//            IFlockSettings        (interface + implementation)
//
//  All boid data lives in a single contiguous TArray<FBoidData>.
//  The state machine, behaviours, formations, and events are all
//  value-type data arrays owned by this struct — no heap objects.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EVOSWARMFLOCK_API FFlock
{
    GENERATED_BODY()

    // ── Identity ─────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName FlockId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsActive = true;

    // ── Simulation timing ────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool  bUseFixedTimestep = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.001"))
    float FixedTimestep = 0.016f;

    float TotalTime     = 0.f;
    float TimestepAccum = 0.f;  // accumulator for fixed-step mode

    // ── Anchor (for StayInRadius) ─────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector AnchorPosition = FVector::ZeroVector;

    // ═══════════════════════════════════════════════════════════════════════
    //  HOT DATA — accessed every tick
    // ═══════════════════════════════════════════════════════════════════════

    /** Primary boid storage — contiguous, cache-friendly. */
    TArray<FBoidData> Boids;

    /** Per-boid neighbor index lists, rebuilt in Pass 1 each tick. */
    TArray<TArray<int32>> NeighborIndices;

    // ── Spatial partitioning ─────────────────────────────────────────────────
    FSpatialGrid SpatialGrid;

    // ── Settings templates (indexed by FBoidData::SettingsIndex) ─────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBoidSettings> SettingsTemplates;

    // ── Default behaviour stack (sorted descending by Priority) ─────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBehaviourEntry> DefaultBehaviours;
    bool bDefaultBehavioursDirty = true;

    // ── State machine data (shared across all boids in this flock) ───────────
    TArray<FStateDefinition> States;
    TArray<FTransitionRule>  Transitions;
    bool bTransitionsDirty = true;

    // ── Formation ────────────────────────────────────────────────────────────
    FFormationState Formation;

    // ── Obstacles & threats (world-space, updated externally) ────────────────
    TArray<FObstacleData> Obstacles;
    TArray<FThreatData>   Threats;

    // ── Event hub ────────────────────────────────────────────────────────────
    FFlockEventHub Events;

    // ═══════════════════════════════════════════════════════════════════════
    //  COLD DATA — genetics (read rarely: on spawn / reproduction)
    // ═══════════════════════════════════════════════════════════════════════

    /** Species-level config: stat ranges, costs, curves, and budget. */
    FFlockSpeciesConfig SpeciesConfig;

    /**
     * Per-boid genome — parallel to Boids.
     * Invariant: BoidGenomes[i].TotalWeight() == SpeciesConfig.TotalStats
     */
    TArray<FFlockGenome> BoidGenomes;

    /**
     * Per-boid runtime stats — parallel to Boids.
     * Recomputed from genome on spawn/mutation; Hp/Stamina/Age mutated each tick.
     */
    TArray<FBoidStats> BoidStats;

    // ═══════════════════════════════════════════════════════════════════════
    //  Helpers
    // ═══════════════════════════════════════════════════════════════════════

    /** Add a new boid and return its index. */
    int32 AddBoid(const FBoidData& InData, const FFlockGenome& InGenome = FFlockGenome{})
    {
        int32 Idx = Boids.Add(InData);
        NeighborIndices.AddDefaulted();  // keep parallel arrays in sync
        BoidGenomes.Add(InGenome);
        // Compute and store stats from genome; caller can override later
        BoidStats.Add(ComputeStatsFromGenome(InGenome, SpeciesConfig));
        ++ActiveCount;
        Events.OnBoidAdded.Broadcast(Idx);
        return Idx;
    }

    /** Deactivate a boid by index (pool-safe: does not shrink the array). */
    void DeactivateBoid(int32 Index)
    {
        if (Boids.IsValidIndex(Index))
        {
            Boids[Index].bIsActive = false;
            Events.OnBoidRemoved.Broadcast(Index);
        }
    }

    /** Return the settings for a given boid (safe, returns a static default if empty). */
    const FBoidSettings& GetSettings(const FBoidData& Boid) const
    {
        if (SettingsTemplates.Num() == 0)
        {
            static const FBoidSettings DefaultSettings;
            ensureMsgf(false, TEXT("FFlock::GetSettings called with empty SettingsTemplates!"));
            return DefaultSettings;
        }
        int32 Idx = FMath::Clamp(Boid.SettingsIndex, 0, SettingsTemplates.Num() - 1);
        return SettingsTemplates[Idx];
    }

    /** Number of currently active boids (maintained manually). */
    int32 ActiveCount = 0;
};
