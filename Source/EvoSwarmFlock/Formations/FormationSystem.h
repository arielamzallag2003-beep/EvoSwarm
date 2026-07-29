#pragma once

#include "CoreMinimal.h"
#include "Formations/FormationTypes.h"

struct FBoidData;

// ─────────────────────────────────────────────────────────────────────────────
//  FormationSystem.h
//
//  Replaces IFormation / IFormationController / LineFormation /
//            CircleFormation / WedgeFormation (all heap objects).
//
//  Two functions:
//    RebuildFormationSlots    — full reassignment (call on config change)
//    UpdateFormationSlotPositions — fast per-tick position update (no reassign)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Assign every active non-leader boid to a slot index, compute world-space
 * slot positions and write them into FBoidData::FormationSlot.
 * Also sets FBoidData::bInFormation and ::LeaderBoidIndex.
 * Call whenever LeaderIndex / Type / Spacing / boid count changes.
 */
void RebuildFormationSlots(FFormationState& Formation, TArray<FBoidData>& Boids);

/**
 * Recompute slot world positions (based on leader's current position/forward)
 * WITHOUT changing slot assignments. Call every tick when Formation.bIsActive.
 */
void UpdateFormationSlotPositions(FFormationState& Formation, TArray<FBoidData>& Boids);
