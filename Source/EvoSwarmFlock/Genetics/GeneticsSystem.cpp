#include "Genetics/GeneticsSystem.h"
#include "Misc/ScopeLock.h"

// ═════════════════════════════════════════════════════════════════════════════
//  Internal helpers
// ═════════════════════════════════════════════════════════════════════════════

/** Seeded LCG rand in [0, Range). Cheaper than FMath::Rand() and reproducible. */
static FORCEINLINE int32 SeededRandRange(uint32& Seed, int32 Range)
{
    if (Range <= 1) return 0;
    Seed = Seed * 1664525u + 1013904223u; // Numerical Recipes LCG
    return static_cast<int32>((Seed >> 1) % static_cast<uint32>(Range));
}

/** Clamp genome weights to [0, TotalStats] per slot, then fix the sum. */
static void NormaliseGenome(FFlockGenome& G, const FFlockSpeciesConfig& S)
{
    // Clamp negatives
    for (int32 i = 0; i < GStatCount; ++i)
        G.Weights[i] = FMath::Max(0, G.Weights[i]);

    // Adjust sum to TotalStats
    int32 Sum   = G.TotalWeight();
    int32 Diff  = Sum - S.TotalStats;

    if (Diff == 0) return;

    // Distribute the imbalance across all slots
    // Use a simple multi-pass greedy approach
    for (int32 Pass = 0; Pass < 10 && Diff != 0; ++Pass)
    {
        for (int32 i = 0; i < GStatCount && Diff != 0; ++i)
        {
            if (Diff > 0)
            {
                int32 Remove = FMath::Min(Diff, FMath::Max(0, G.Weights[i] - 1));
                G.Weights[i] -= Remove;
                Diff -= Remove;
            }
            else
            {
                G.Weights[i] += -Diff;
                Diff = 0;
            }
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  MakeDefaultSpeciesConfig
//  Implements the agreed stat table (CostToFill + Curve + Min/Max per slot)
// ═════════════════════════════════════════════════════════════════════════════
FFlockSpeciesConfig MakeDefaultSpeciesConfig()
{
    FFlockSpeciesConfig Cfg;
    Cfg.TotalStats      = 1000;
    Cfg.StaminaDrainRate = 20.f;
    Cfg.StaminaRegenRate = 10.f;
    Cfg.SprintThreshold  = 0.70f;

    // ── Slot 0: Hp ────────────────────────────────────────────────────────────
    Cfg.StatConfigs[0] = { 1.f,   500.f,  200, EStatCurve::Linear,  TEXT("Hp")                };
    // ── Slot 1: Armure ────────────────────────────────────────────────────────
    Cfg.StatConfigs[1] = { 0.f,   100.f,  200, EStatCurve::Convex,  TEXT("Armure")             };
    // ── Slot 2: VitesseMarche ─────────────────────────────────────────────────
    Cfg.StatConfigs[2] = { 0.5f,   20.f,  150, EStatCurve::Convex,  TEXT("VitesseMarche")      };
    // ── Slot 3: VitesseCourse ─────────────────────────────────────────────────
    Cfg.StatConfigs[3] = { 1.f,    40.f,  250, EStatCurve::Convex,  TEXT("VitesseCourse")      };
    // ── Slot 4: Stamina ───────────────────────────────────────────────────────
    Cfg.StatConfigs[4] = { 1.f,   200.f,  100, EStatCurve::Linear,  TEXT("Stamina")            };
    // ── Slot 5: Regeneration ──────────────────────────────────────────────────
    Cfg.StatConfigs[5] = { 0.f,     5.f,  100, EStatCurve::Convex,  TEXT("Regeneration")       };
    // ── Slot 6: Faim ──────────────────────────────────────────────────────────
    Cfg.StatConfigs[6] = { 10.f,  500.f,   80, EStatCurve::Linear,  TEXT("Faim")               };
    // ── Slot 7: Biomasse ──────────────────────────────────────────────────────
    Cfg.StatConfigs[7] = { 1.f,   100.f,   60, EStatCurve::Linear,  TEXT("Biomasse")           };
    // ── Slot 8: Discretion ────────────────────────────────────────────────────
    Cfg.StatConfigs[8] = { 0.f,     1.f,   80, EStatCurve::Concave, TEXT("Discretion")         };
    // ── Slot 9: Degats ────────────────────────────────────────────────────────
    Cfg.StatConfigs[9] = { 0.f,   200.f,  300, EStatCurve::Convex,  TEXT("Degats")             };
    // ── Slot 10: Intimidation ─────────────────────────────────────────────────
    Cfg.StatConfigs[10]= { 0.f,     1.f,   80, EStatCurve::Concave, TEXT("Intimidation")       };
    // ── Slot 11: Agressivite ──────────────────────────────────────────────────
    Cfg.StatConfigs[11]= { 0.f,     1.f,  100, EStatCurve::Linear,  TEXT("Agressivite")        };
    // ── Slot 12: Perception ───────────────────────────────────────────────────
    Cfg.StatConfigs[12]= { 1.f,    50.f,  120, EStatCurve::Convex,  TEXT("Perception")         };
    // ── Slot 13: RegimeAlimentaire ────────────────────────────────────────────
    Cfg.StatConfigs[13]= { 0.f,     1.f,   40, EStatCurve::Linear,  TEXT("RegimeAlimentaire")  };
    // ── Slot 14: TempsDeVie ───────────────────────────────────────────────────
    Cfg.StatConfigs[14]= { 10.f, 3600.f,  150, EStatCurve::Convex,  TEXT("TempsDeVie")         };
    // ── Slot 15: TauxDeReproduction ───────────────────────────────────────────
    Cfg.StatConfigs[15]= { 0.f,     1.f,  150, EStatCurve::Convex,  TEXT("TauxDeReproduction") };
    // ── Slot 16: Integration ──────────────────────────────────────────────────
    Cfg.StatConfigs[16]= { 0.f,     1.f,  100, EStatCurve::Linear,  TEXT("Integration")        };
    // ── Slot 17: TauxDeMutation ───────────────────────────────────────────────
    Cfg.StatConfigs[17]= { 0.f,     1.f,   40, EStatCurve::Linear,  TEXT("TauxDeMutation")     };

    // Default genome: equal distribution (55 per slot + 10 remainder on slot 0)
    int32 Base = Cfg.TotalStats / GStatCount;           // 55
    int32 Rem  = Cfg.TotalStats - Base * GStatCount;    // 10
    for (int32 i = 0; i < GStatCount; ++i)
        Cfg.DefaultGenome.Weights[i] = Base + (i < Rem ? 1 : 0);

    return Cfg;
}

// ═════════════════════════════════════════════════════════════════════════════
//  ComputeStatsFromGenome
// ═════════════════════════════════════════════════════════════════════════════
FBoidStats ComputeStatsFromGenome(const FFlockGenome& Genome, const FFlockSpeciesConfig& Species)
{
    // Helper lambda: weight → actual value using the slot's curve + range
    auto Eval = [&](EStatSlot Slot) -> float
    {
        const int32 s   = static_cast<int32>(Slot);
        const FStatConfig& Cfg = Species.StatConfigs[s];
        const float rawT = (Cfg.CostToFill > 0)
            ? static_cast<float>(Genome.Weights[s]) / static_cast<float>(Cfg.CostToFill)
            : 0.f;
        const float t = ApplyCurve(rawT, Cfg.Curve);
        return FMath::Lerp(Cfg.Min, Cfg.Max, t);
    };

    FBoidStats S;

    // ── Combat / survival ─────────────────────────────────────────────────────
    S.MaxHp      = FMath::RoundToInt(Eval(EStatSlot::Hp));
    S.Hp         = S.MaxHp;   // full health on spawn

    S.Armure     = FMath::RoundToInt(Eval(EStatSlot::Armure));

    // ── Movement ──────────────────────────────────────────────────────────────
    S.VitesseMarche  = Eval(EStatSlot::VitesseMarche);
    S.VitesseCourse  = Eval(EStatSlot::VitesseCourse);

    S.MaxStamina = Eval(EStatSlot::Stamina);
    S.Stamina    = S.MaxStamina;

    S.Regeneration = Eval(EStatSlot::Regeneration);

    // ── Ecology ───────────────────────────────────────────────────────────────
    S.MaxFaim  = FMath::RoundToInt(Eval(EStatSlot::Faim));
    S.Faim     = 0;   // hungry on spawn
    S.Biomasse = FMath::RoundToInt(Eval(EStatSlot::Biomasse));

    // ── Behaviour multipliers ─────────────────────────────────────────────────
    S.Discretion    = Eval(EStatSlot::Discretion);
    S.Degats        = FMath::RoundToInt(Eval(EStatSlot::Degats));
    S.Intimidation  = Eval(EStatSlot::Intimidation);
    S.Agressivite   = Eval(EStatSlot::Agressivite);
    S.Perception    = Eval(EStatSlot::Perception);

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    S.RegimeAlimentaire  = Eval(EStatSlot::RegimeAlimentaire);
    S.TempsDeVie         = Eval(EStatSlot::TempsDeVie);
    S.Age                = 0.f;
    S.TauxDeReproduction = Eval(EStatSlot::TauxDeReproduction);
    S.ReproductionAccum  = 0.f;
    S.Integration        = Eval(EStatSlot::Integration);
    S.TauxDeMutation     = Eval(EStatSlot::TauxDeMutation);

    return S;
}

// ═════════════════════════════════════════════════════════════════════════════
//  ApplyStatsToSettings
//  Push genome-derived stats into FBoidSettings so the existing sim uses them.
// ═════════════════════════════════════════════════════════════════════════════
void ApplyStatsToSettings(const FBoidStats& Stats, FBoidSettings& Out, bool bIsSprinting)
{
    Out.MaxSpeed         = bIsSprinting ? Stats.VitesseCourse : Stats.VitesseMarche;
    Out.PerceptionRadius = Stats.Perception;
    // FieldOfViewAngle, Mass, Drag, ObstacleAvoidanceDistance remain species-level
    // and are NOT overridden here; the caller controls them via UBoidSettingsAsset.
}

// ═════════════════════════════════════════════════════════════════════════════
//  MutateGenome
// ═════════════════════════════════════════════════════════════════════════════
FFlockGenome MutateGenome(
    const FFlockGenome& Parent, const FFlockSpeciesConfig& Species,
    float MutationRate, uint32& Seed)
{
    FFlockGenome Child = Parent;

    // Number of point-transfer operations
    int32 Ops = FMath::Max(1, FMath::RoundToInt(MutationRate * GStatCount));

    for (int32 Op = 0; Op < Ops; ++Op)
    {
        // Pick two distinct slots
        int32 From = SeededRandRange(Seed, GStatCount);
        int32 To   = SeededRandRange(Seed, GStatCount - 1);
        if (To >= From) ++To; // skip same slot

        // Transfer a random portion of the donor slot
        if (Child.Weights[From] <= 0) continue;
        int32 MaxDelta = FMath::Max(1, Child.Weights[From] / 2);
        int32 Delta    = 1 + SeededRandRange(Seed, MaxDelta);

        Child.Weights[From] -= Delta;
        Child.Weights[To]   += Delta;
    }

    // Re-normalise to fix any clamp violations
    NormaliseGenome(Child, Species);
    return Child;
}

// ═════════════════════════════════════════════════════════════════════════════
//  MateGenomes
//  Per-slot coin-flip inheritance, then normalise, then mutate
// ═════════════════════════════════════════════════════════════════════════════
FFlockGenome MateGenomes(
    const FFlockGenome& A, const FFlockGenome& B,
    const FFlockSpeciesConfig& Species, uint32& Seed)
{
    FFlockGenome Child;

    // Randomly inherit each slot from either parent (uniform crossover)
    for (int32 i = 0; i < GStatCount; ++i)
    {
        Seed = Seed * 1664525u + 1013904223u;
        Child.Weights[i] = (Seed & 1u) ? A.Weights[i] : B.Weights[i];
    }

    // Normalise (crossover rarely produces exact sum)
    NormaliseGenome(Child, Species);

    // Use average mutation rate of both parents then mutate
    const float AvgMutRate =
        0.5f * (static_cast<float>(A.Weights[static_cast<int32>(EStatSlot::TauxDeMutation)])
                / static_cast<float>(FMath::Max(1, Species.StatConfigs[static_cast<int32>(EStatSlot::TauxDeMutation)].CostToFill))
              + static_cast<float>(B.Weights[static_cast<int32>(EStatSlot::TauxDeMutation)])
                / static_cast<float>(FMath::Max(1, Species.StatConfigs[static_cast<int32>(EStatSlot::TauxDeMutation)].CostToFill)));

    return MutateGenome(Child, Species, AvgMutRate, Seed);
}

// ═════════════════════════════════════════════════════════════════════════════
//  ValidateGenome
// ═════════════════════════════════════════════════════════════════════════════
bool ValidateGenome(const FFlockGenome& Genome, const FFlockSpeciesConfig& Species)
{
    int32 Sum = 0;
    for (int32 i = 0; i < GStatCount; ++i)
    {
        if (Genome.Weights[i] < 0)
        {
            ensureMsgf(false, TEXT("GeneticsSystem: slot %d has negative weight %d"), i, Genome.Weights[i]);
            return false;
        }
        Sum += Genome.Weights[i];
    }
    if (Sum != Species.TotalStats)
    {
        ensureMsgf(false, TEXT("GeneticsSystem: genome sum %d != TotalStats %d"), Sum, Species.TotalStats);
        return false;
    }
    return true;
}
