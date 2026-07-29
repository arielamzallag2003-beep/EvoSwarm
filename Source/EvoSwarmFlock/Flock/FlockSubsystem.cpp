#include "Flock/FlockSubsystem.h"
#include "Flock/SpatialGrid.h"
#include "Behaviours/BehaviourSystem.h"
#include "StateMachine/StateMachineSystem.h"
#include "Formations/FormationSystem.h"
#include "Genetics/GeneticsSystem.h"
#include "Engine/World.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────────────────────────────────────
void UFlockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Register a per-frame tick via the global ticker (works in any world)
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UFlockSubsystem::TickFromDelegate),
        0.f);
}

void UFlockSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    AllFlocks.Empty();
    Super::Deinitialize();
}

void UFlockSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
}

bool UFlockSubsystem::TickFromDelegate(float DeltaTime)
{
    Tick(DeltaTime);
    return true; // keep ticking
}

// ─────────────────────────────────────────────────────────────────────────────
//  Flock Management
// ─────────────────────────────────────────────────────────────────────────────
int32 UFlockSubsystem::CreateFlock(FName FlockId)
{
    FFlock& NewFlock = AllFlocks.AddDefaulted_GetRef();
    NewFlock.FlockId = FlockId;
    NewFlock.SettingsTemplates.AddDefaulted(); // index 0 = default settings
    NewFlock.SpeciesConfig = MakeDefaultSpeciesConfig(); // seed default genetics
    return AllFlocks.Num() - 1;
}

bool UFlockSubsystem::DestroyFlock(int32 FlockIndex)
{
    if (!AllFlocks.IsValidIndex(FlockIndex)) return false;
    AllFlocks.RemoveAt(FlockIndex);
    return true;
}

int32 UFlockSubsystem::AddBoidToFlock(int32 FlockIndex, const FBoidData& InitialData)
{
    FFlock* Flock = GetFlock(FlockIndex);
    if (!Flock) return INDEX_NONE;

    int32 Idx = Flock->Boids.Add(InitialData);
    Flock->NeighborIndices.AddDefaulted();
    ++Flock->ActiveCount;
    Flock->Events.OnBoidAdded.Broadcast(Idx);
    return Idx;
}

void UFlockSubsystem::DeactivateBoid(int32 FlockIndex, int32 BoidIndex)
{
    FFlock* Flock = GetFlock(FlockIndex);
    if (!Flock || !Flock->Boids.IsValidIndex(BoidIndex)) return;
    if (Flock->Boids[BoidIndex].bIsActive)
    {
        Flock->Boids[BoidIndex].bIsActive = false;
        --Flock->ActiveCount;
        Flock->Events.OnBoidRemoved.Broadcast(BoidIndex);
    }
}

FFlock* UFlockSubsystem::GetFlock(int32 FlockIndex)
{
    return AllFlocks.IsValidIndex(FlockIndex) ? &AllFlocks[FlockIndex] : nullptr;
}

const FFlock* UFlockSubsystem::GetFlock(int32 FlockIndex) const
{
    return AllFlocks.IsValidIndex(FlockIndex) ? &AllFlocks[FlockIndex] : nullptr;
}

bool UFlockSubsystem::GetBoidTransform(
    int32 FlockIndex, int32 BoidIndex,
    FVector& OutPosition, FVector& OutForward) const
{
    const FFlock* Flock = GetFlock(FlockIndex);
    if (!Flock || !Flock->Boids.IsValidIndex(BoidIndex)) return false;
    const FBoidData& B = Flock->Boids[BoidIndex];
    OutPosition = B.Position;
    OutForward  = B.Forward;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main Tick
// ─────────────────────────────────────────────────────────────────────────────
void UFlockSubsystem::Tick(float DeltaTime)
{
    for (FFlock& Flock : AllFlocks)
    {
        if (!Flock.bIsActive || Flock.Boids.Num() == 0) continue;

        if (Flock.bUseFixedTimestep)
        {
            Flock.TimestepAccum += DeltaTime;
            while (Flock.TimestepAccum >= Flock.FixedTimestep)
            {
                TickFlock(Flock, Flock.FixedTimestep);
                Flock.TimestepAccum -= Flock.FixedTimestep;
            }
        }
        else
        {
            TickFlock(Flock, DeltaTime);
        }

        // Flush deferred events after simulation is done
        Flock.Events.Flush();
    }
}

void UFlockSubsystem::TickFlock(FFlock& Flock, float DeltaTime)
{
    Flock.TotalTime += DeltaTime;

    // ── Sort on first use / dirty flag ──────────────────────────────────────
    if (Flock.bDefaultBehavioursDirty)
    {
        Flock.DefaultBehaviours.Sort(FBehaviourPrioritySorter{});
        Flock.bDefaultBehavioursDirty = false;
    }
    if (Flock.bTransitionsDirty)
    {
        SortStateMachineData(Flock.States, Flock.Transitions);
        Flock.bTransitionsDirty = false;
    }

    // ── Formation slot rebuild if needed ────────────────────────────────────
    if (Flock.Formation.bIsActive)
    {
        if (Flock.Formation.bSlotsDirty)
            RebuildFormationSlots(Flock.Formation, Flock.Boids);
        else
            UpdateFormationSlotPositions(Flock.Formation, Flock.Boids);
    }

    Pass_RebuildSpatial(Flock);
    Pass_ComputeForces(Flock, DeltaTime);
    Pass_Integrate(Flock, DeltaTime);
    Pass_UpdateStats(Flock, DeltaTime);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pass 1 — Spatial + Neighbor Lists
//  Replaces: Flock.GetNeighbors LINQ chain (called per-boid per-tick)
// ─────────────────────────────────────────────────────────────────────────────
void UFlockSubsystem::Pass_RebuildSpatial(FFlock& Flock)
{
    // Use the maximum perception radius as the grid cell size
    float MaxRadius = 10.f;
    for (const FBoidSettings& S : Flock.SettingsTemplates)
        MaxRadius = FMath::Max(MaxRadius, S.PerceptionRadius);

    Flock.SpatialGrid.Rebuild(Flock.Boids, MaxRadius);

    // Fill per-boid neighbor index lists
    for (int32 i = 0; i < Flock.Boids.Num(); ++i)
    {
        const FBoidData& B = Flock.Boids[i];
        if (!B.bIsActive)
        {
            Flock.NeighborIndices[i].Reset();
            continue;
        }

        const FBoidSettings& S = Flock.GetSettings(B);
        Flock.SpatialGrid.QueryNeighbors(
            Flock.Boids, i,
            S.PerceptionRadius, S.MaxNeighbors,
            Flock.NeighborIndices[i]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pass 2 — Force Computation
//  Replaces: Flock.Step inner loop + FlockStateMachineBehaviour.CalculateForce
// ─────────────────────────────────────────────────────────────────────────────
void UFlockSubsystem::Pass_ComputeForces(FFlock& Flock, float DeltaTime)
{
    for (int32 i = 0; i < Flock.Boids.Num(); ++i)
    {
        FBoidData& Boid = Flock.Boids[i];
        if (!Boid.bIsActive) continue;

        const FBoidSettings& Settings = Flock.GetSettings(Boid);

        // Build stack-allocated context (replaces heap-allocated BoidContext)
        FBoidContext Ctx;
        Ctx.Self          = &Boid;
        Ctx.Settings      = &Settings;
        Ctx.NeighborIdx   = &Flock.NeighborIndices[i];
        Ctx.AllBoids      = &Flock.Boids;
        Ctx.Obstacles     = &Flock.Obstacles;
        Ctx.Threats       = &Flock.Threats;
        Ctx.DeltaTime     = DeltaTime;
        Ctx.TotalTime     = Flock.TotalTime;
        Ctx.AnchorPosition = Flock.AnchorPosition;

        // ── State machine update ──────────────────────────────────────────
        int32 PrevState = INDEX_NONE;
        if (Flock.States.Num() > 0)
        {
            UpdateStateMachine(Boid, Flock.States, Flock.Transitions, Ctx, PrevState);
            if (PrevState != INDEX_NONE)
            {
                // Queue state-change event (fired post-tick)
                Flock.Events.Push({
                    EFlockEventType::StateChanged,
                    i,
                    static_cast<int32>(Boid.StateIndex)
                });
            }
        }

        // ── Determine behaviour stack for current state ───────────────────
        const TArray<FBehaviourEntry>* BehaviourStack = &Flock.DefaultBehaviours;
        if (Flock.States.IsValidIndex(Boid.StateIndex))
            BehaviourStack = &Flock.States[Boid.StateIndex].Behaviours;

        // ── Accumulate forces (priority-dampened, replaces Flock.Step) ────
        Boid.AccumulatedForce = FVector::ZeroVector;
        FVector TotalForce        = FVector::ZeroVector;
        float   RemainingAuthority = 1.f;

        for (const FBehaviourEntry& Entry : *BehaviourStack)
        {
            if (!Entry.Params.bEnabled || RemainingAuthority < 0.01f) break;

            FVector F   = CalculateBehaviourForce(Entry.Params, Boid, Settings, Ctx)
                          * Entry.Params.Weight;
            float   Mag = F.Size();

            TotalForce += F * RemainingAuthority;

            // Authority dampening (mirrors C# logic exactly)
            if (Entry.Params.Priority > 0 && Mag > 0.1f)
            {
                float Damp = (Entry.Params.Priority / 20.f)
                           * (Mag / FMath::Max(Settings.MaxSpeed, 0.001f));
                RemainingAuthority = FMath::Max(0.f, RemainingAuthority - Damp);
            }
        }

        Boid.AccumulatedForce = TotalForce;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pass 3 — Integration
//  Replaces: IBoid.ApplyForces
//  Separate pass = SIMD-friendly (all reads then all writes).
// ─────────────────────────────────────────────────────────────────────────────
void UFlockSubsystem::Pass_Integrate(FFlock& Flock, float DeltaTime)
{
    for (FBoidData& Boid : Flock.Boids)
    {
        if (!Boid.bIsActive) continue;

        const FBoidSettings& S = Flock.GetSettings(Boid);

        // Clamp force to MaxForce / mass
        FVector ClampedForce = Boid.AccumulatedForce.GetClampedToMaxSize(S.MaxForce / S.Mass);

        // Euler integration
        Boid.Velocity += ClampedForce * DeltaTime;
        Boid.Velocity *= (1.f - S.Drag * DeltaTime);                      // drag
        Boid.Velocity  = Boid.Velocity.GetClampedToMaxSize(S.MaxSpeed);   // speed cap

        EnforcePlaneConstraint(Boid, S.MovementPlane);

        Boid.Position += Boid.Velocity * DeltaTime;

        if (!Boid.Velocity.IsNearlyZero(1e-4f))
            Boid.Forward = Boid.Velocity.GetSafeNormal();

        Boid.AccumulatedForce = FVector::ZeroVector;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
void UFlockSubsystem::EnforcePlaneConstraint(FBoidData& Boid, EMovementPlane Plane)
{
    switch (Plane)
    {
    case EMovementPlane::XZ:
        Boid.Velocity.Y = 0.f;
        break;
    case EMovementPlane::XY:
        Boid.Velocity.Z = 0.f;
        break;
    case EMovementPlane::Full:
    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  AddBoidToFlockWithGenome
// ─────────────────────────────────────────────────────────────────────────────
int32 UFlockSubsystem::AddBoidToFlockWithGenome(
    int32 FlockIndex, const FBoidData& InitialData, const FFlockGenome& Genome)
{
    FFlock* Flock = GetFlock(FlockIndex);
    if (!Flock) return INDEX_NONE;
    return Flock->AddBoid(InitialData, Genome);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pass 4 — Stats Update
//  Runs AFTER Pass_Integrate each tick.
//  Handles: sprinting toggle, stamina, HP regen, aging, death, reproduction.
// ─────────────────────────────────────────────────────────────────────────────
void UFlockSubsystem::Pass_UpdateStats(FFlock& Flock, float DeltaTime)
{
    const FFlockSpeciesConfig& Species = Flock.SpeciesConfig;

    for (int32 i = 0; i < Flock.Boids.Num(); ++i)
    {
        FBoidData& Boid = Flock.Boids[i];
        if (!Boid.bIsActive) continue;

        // Parallel arrays guard
        if (!Flock.BoidStats.IsValidIndex(i)) continue;
        FBoidStats& Stats = Flock.BoidStats[i];

        // ──────────────────────────────────
        //  Sprint toggling + Stamina
        // ──────────────────────────────────
        const FBoidSettings& S = Flock.GetSettings(Boid);
        const float SpeedFraction = Boid.Velocity.Size() /
                                    FMath::Max(Stats.VitesseCourse, 0.001f);

        if (Boid.bIsSprinting)
        {
            // Drain stamina while sprinting
            Stats.Stamina -= Species.StaminaDrainRate * DeltaTime;
            if (Stats.Stamina <= 0.f)
            {
                Stats.Stamina    = 0.f;
                Boid.bIsSprinting = false; // out of stamina: drop to walk
            }
        }
        else
        {
            // Restore stamina while not sprinting
            Stats.Stamina = FMath::Min(
                Stats.MaxStamina,
                Stats.Stamina + Species.StaminaRegenRate * DeltaTime);

            // Re-enable sprinting if stamina recovered above 20%
            if (SpeedFraction >= Species.SprintThreshold &&
                Stats.Stamina > Stats.MaxStamina * 0.2f)
            {
                Boid.bIsSprinting = true;
            }
        }

        // Update MaxSpeed in runtime settings based on sprint state
        // (We write directly into a mutable settings entry for this boid)
        ApplyStatsToSettings(Stats, Flock.SettingsTemplates[FMath::Clamp(
            Boid.SettingsIndex, 0, Flock.SettingsTemplates.Num() - 1)],
            Boid.bIsSprinting);

        // ──────────────────────────────────
        //  HP regeneration
        // ──────────────────────────────────
        Stats.Hp = FMath::Min(
            Stats.MaxHp,
            Stats.Hp + FMath::RoundToInt(Stats.Regeneration * DeltaTime));

        // ──────────────────────────────────
        //  Aging + death
        // ──────────────────────────────────
        Stats.Age += DeltaTime;

        const bool bOldAge = Stats.Age >= Stats.TempsDeVie;
        const bool bKilled = Stats.Hp <= 0;

        if (bOldAge || bKilled)
        {
            Flock.DeactivateBoid(i);
            continue;
        }

        // ──────────────────────────────────
        //  Reproduction
        //  Accumulate time; spawn a child when the threshold is crossed.
        //  Threshold = 1 / TauxDeReproduction seconds (higher repro = faster).
        // ──────────────────────────────────
        if (Stats.TauxDeReproduction > 0.f)
        {
            Stats.ReproductionAccum += DeltaTime;
            const float SpawnInterval = 1.f / Stats.TauxDeReproduction;

            if (Stats.ReproductionAccum >= SpawnInterval)
            {
                Stats.ReproductionAccum -= SpawnInterval;
                SpawnChild(Flock, i);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SpawnChild — create a mutated boid near the parent
// ─────────────────────────────────────────────────────────────────────────────
void UFlockSubsystem::SpawnChild(FFlock& Flock, int32 ParentIndex)
{
    if (!Flock.BoidGenomes.IsValidIndex(ParentIndex)) return;

    const FFlockGenome& ParentGenome = Flock.BoidGenomes[ParentIndex];
    const FBoidStats&  ParentStats  = Flock.BoidStats[ParentIndex];
    const FBoidData&   ParentBoid   = Flock.Boids[ParentIndex];

    // Mutate the parent genome using the parent's own TauxDeMutation
    FFlockGenome ChildGenome = MutateGenome(
        ParentGenome,
        Flock.SpeciesConfig,
        ParentStats.TauxDeMutation,
        Flock.Boids[ParentIndex].RandSeed);

    // Spawn slightly offset from parent
    FBoidData ChildBoid;
    ChildBoid.Position    = ParentBoid.Position + FVector(
        FMath::RandRange(-50.f, 50.f),
        FMath::RandRange(-50.f, 50.f),
        0.f);
    ChildBoid.Forward     = ParentBoid.Forward;
    ChildBoid.bIsActive   = 1;
    ChildBoid.RandSeed    = ParentBoid.RandSeed ^ static_cast<uint32>(Flock.TotalTime * 1000.f);

    Flock.AddBoid(ChildBoid, ChildGenome);
}
