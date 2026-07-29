// Copyright Evoswarm.
//
// Per-world simulation services that must run OUTSIDE Mass processing: spawning boids
// and food, flushing queued births from the reproduction processor, and maintaining the
// food field. Also the registry mapping species index -> render ISM. Ticking here (not
// inside a processor) keeps all structural entity changes off the Mass execution path.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityHandle.h" // Latest include order no longer pulls this in via MassEntityTypes.h
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h" // NumDietHues, StatsSampleInterval, ... (all inline constexpr)
#include "EvoswarmAnalytics.h" // whole-run timelines, trophic ledger, analytics pages
#include "EvoswarmSimSubsystem.generated.h"

class UInstancedStaticMeshComponent;
class UAudioComponent;
class USoundWaveProcedural;
struct FMassEntityManager;

/** A queued offspring, produced by the reproduction processor, born on the next sim tick. */
struct FBoidBirthRequest
{
	FBoidSpeciesSharedFragment Shared;
	FBoidGenome Genome;
	FVector Position = FVector::ZeroVector;
	int32 Generation = 0;
};

/** Why a boid died. Predation = eaten outright; Injury = combat wounds (incl. counter-attacks). */
enum class EDeathCause : uint8
{
	Starvation,
	OldAge,
	Predation,
	Injury,

	Count
};
static constexpr int32 NumDeathCauses = static_cast<int32>(EDeathCause::Count);

/** One entry in the ecosystem event feed (extinctions, milestones, diet shifts, collapses). */
struct FSimEvent
{
	float Time = 0.f;   // sim time (seconds) when it happened
	FString Text;
	FLinearColor Color = FLinearColor::White;
};

/** Snapshot of one creature's live data for the crosshair inspector panel. */
struct FBoidInspectState
{
	bool bActive = false;          // anything being inspected?
	bool bLocked = false;          // user pressed the select key
	bool bDeceased = false;        // locked entity was destroyed

	FMassEntityHandle Entity;
	int32 SpeciesIndex = -1;
	FVector Position = FVector::ZeroVector;

	// Copied from fragments each frame by the pawn; frozen when deceased.
	FBoidGenome Genome;
	float HP = 0.f;
	float MaxHP = 0.f;
	float Stam = 0.f;
	float MaxStam = 0.f;
	float Hunger = 0.f;
	float MaxHunger = 0.f;
	float Age = 0.f;
	float Lifespan = 0.f;
	int32 Generation = 0;
	int32 ReproCount = 0;
	float Adrenaline = 0.f;
	float ReproCooldown = 0.f;
	float AttackCooldown = 0.f;
};

/** Per-species live readout, refreshed each frame by the stats processor and shown on the HUD. */
struct FSpeciesLiveStats
{
	FString Name;
	FLinearColor Color = FLinearColor::White;
	int32 Count = 0;
	// Population-averaged genome values (let you watch evolution drift).
	float AvgHP = 0.f;
	float AvgWalkSpeed = 0.f;
	float AvgPerception = 0.f;
	float AvgDiet = 0.f;
	float AvgLifespan = 0.f;
	float AvgDamage = 0.f;
	float AvgArmor = 0.f;
	float AvgMutationRate = 0.f;
	float AvgGeneration = 0.f;
	int32 MaxGeneration = 0;

	/**
	 * Every stat's full live distribution (mean / sd / min / max), refreshed each frame by the
	 * stats processor. The Avg* fields above are just TraitNow[..].Mean, kept because the
	 * existing panel rows read them by name. The spread is the point: a mean alone cannot tell
	 * a uniform population from one that has split into two strategies.
	 */
	FTraitDistribution TraitNow[NumBoidStats];

	/**
	 * A subsample of live genomes (stride-picked, up to Evo::ScatterMaxPoints) so the scatter
	 * page can plot individuals rather than averages. Whole genomes, not one chosen pair, so
	 * switching axes at runtime costs nothing on the sim side.
	 */
	TArray<FBoidGenome> GenomeSamples;

	/** Whole-run history: mean and spread of every stat, decimated to stay memory-bounded. */
	FSpeciesTimeline Timeline;

	/** Who this species ate, and how much energy it actually got out of it. */
	FTrophicLedger Trophic;

	/** Recent population samples (ring buffer) for the HUD sparkline; oldest first. */
	TArray<int32> PopHistory;

	/**
	 * Diet spread across Evo::NumDietHues bins (same bins as the render hues), refreshed each
	 * frame. This shows speciation the average hides: a species splitting into a herbivore and
	 * a carnivore sub-population reads as two humps here, but as "omnivore" in AvgDiet.
	 */
	TArray<int32> DietHistogram;

	// --- Lifetime totals (since sim start) ---
	int32 TotalBirths = 0;
	int32 TotalKillsMade = 0;                 // prey this species has killed
	int32 Deaths[NumDeathCauses] = { 0 };     // indexed by EDeathCause

	int32 DeathCount(EDeathCause Cause) const { return Deaths[static_cast<int32>(Cause)]; }
	int32 TotalDeaths() const
	{
		int32 Sum = 0;
		for (int32 I = 0; I < NumDeathCauses; ++I) { Sum += Deaths[I]; }
		return Sum;
	}

	/** Births / deaths per sampling interval, aligned with PopHistory (oldest first). */
	TArray<int32> BirthHistory;
	TArray<int32> DeathHistory;

	// --- Internal bookkeeping for sampling + event detection (not for display) ---
	int32 LastSampledBirths = 0;     // TotalBirths at the previous history sample
	int32 LastSampledDeaths = 0;     // TotalDeaths() at the previous history sample
	int32 LastMilestoneGen = 0;      // last generation milestone already logged
	bool bWasAlive = false;          // had a living population (for extinction detection)
	float LastCollapseLogTime = -1.e9f; // throttles repeated collapse events
	int32 DietClass = 1;             // 0 = herbivore, 1 = omnivore, 2 = carnivore
	bool bDietClassInit = false;     // first classification is silent (no "turned X" event)
};

namespace Evo
{
	/** Events per minute over the most recent WindowSec of a per-sample history. */
	inline float RatePerMinute(const TArray<int32>& History, float WindowSec = RateWindowSec)
	{
		const int32 N = History.Num();
		if (N == 0)
		{
			return 0.f;
		}
		const int32 WindowSamples = FMath::Max(1, FMath::RoundToInt(WindowSec / StatsSampleInterval));
		const int32 Use = FMath::Min(WindowSamples, N);
		int32 Sum = 0;
		for (int32 I = N - Use; I < N; ++I)
		{
			Sum += History[I];
		}
		const float Seconds = Use * StatsSampleInterval;
		return (Seconds > 0.f) ? (Sum * 60.f / Seconds) : 0.f;
	}
}

UCLASS()
class EVOSWARM_API UEvoswarmSimSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Setup, called by the game mode ---
	// Boids are rendered in diet-colour buckets (one instanced mesh per bucket), so an
	// individual's colour reflects its own genome rather than a fixed per-species colour.
	void RegisterBoidBucketISM(int32 Bucket, UInstancedStaticMeshComponent* ISM);
	UInstancedStaticMeshComponent* GetBoidBucketISM(int32 Bucket) const;
	int32 NumBoidBuckets() const { return BoidBucketISM.Num(); }

	int32 NumSpecies() const { return SpeciesStats.Num(); }

	void RegisterFoodISM(UInstancedStaticMeshComponent* ISM) { FoodISM = ISM; }
	UInstancedStaticMeshComponent* GetFoodISM() const { return FoodISM; }

	void RegisterCarcassISM(UInstancedStaticMeshComponent* ISM) { CarcassISM = ISM; }
	UInstancedStaticMeshComponent* GetCarcassISM() const { return CarcassISM; }

	// --- Live stats (for the HUD) ---
	void SetSpeciesInfo(int32 SpeciesIndex, const FString& Name, const FLinearColor& Color);
	const TArray<FSpeciesLiveStats>& GetSpeciesStats() const { return SpeciesStats; }
	TArray<FSpeciesLiveStats>& GetSpeciesStatsMutable() { return SpeciesStats; }
	int32 GetFoodCount() const { return FoodCount; }

	/** Live food census from the stats processor (display only; regrowth still uses FoodCount). */
	void SetFoodCensus(int32 Plants, int32 Carcasses) { LivePlantCount = Plants; LiveCarcassCount = Carcasses; }
	int32 GetLivePlantCount() const { return LivePlantCount; }
	int32 GetLiveCarcassCount() const { return LiveCarcassCount; }

	/** Kills by Killer on Victim since sim start. Drives the food-web page's arrows. */
	int32 GetKillCount(int32 Killer, int32 Victim) const;

	/** Picks a plant spot inside one of the fertile meadow patches (false if none fits). */
	bool SamplePlantLocation(FVector& OutLocation);

	/** Current population of a species (from the stats processor; ~1 frame stale). */
	int32 GetSpeciesCount(int32 SpeciesIndex) const { return SpeciesStats.IsValidIndex(SpeciesIndex) ? SpeciesStats[SpeciesIndex].Count : 0; }

	/** Enable food regrowth + birth flushing. The game mode calls this once the world is built. */
	void StartSimulation();

	// --- Accélérateur de temps (+ / - / molette montent et descendent, P met en pause) ---
	// L'idée : la dilatation globale d'UE fait avancer la simulation N fois plus vite, et le
	// mode turbo allège le rendu pour que les frames restent courtes — ce qui garde le PAS
	// d'intégration petit et donc la nuée crédible même à x50.
	void CycleSpeed(int32 Direction);
	void SetSpeedIndex(int32 Index);
	void TogglePause();

	bool IsPaused() const { return bPaused; }
	int32 GetSpeedIndex() const { return SpeedIndex; }
	/** Palier demandé par l'utilisateur (0 en pause). */
	float GetRequestedSpeed() const { return bPaused ? 0.f : Evo::SpeedLadder[FMath::Clamp(SpeedIndex, 0, Evo::NumSpeedLevels - 1)]; }
	/** Facteur réellement appliqué au monde après auto-régulation. */
	float GetEffectiveSpeed() const { return EffectiveSpeed; }
	/** Secondes de simulation écoulées lors de la dernière frame (mesure de finesse du pas). */
	float GetSimStepSeconds() const { return LastSimStep; }
	bool IsTurboActive() const { return bTurboActive; }

	// --- Debug visualisation (B toggles it on/off, numpad 0-4 picks the overlay) ---
	void ToggleDebugDraw() { bDebugDraw = !bDebugDraw; }
	bool IsDebugDraw() const { return bDebugDraw; }

	/** 0 = markers, 1 = perception, 2 = behaviour state, 3 = vitals, 4 = everything. */
	void SetDebugMode(int32 NewMode);
	int32 GetDebugMode() const { return CurrentDebugMode; }

	// --- Analytics dashboard (G opens it, Tab pages, [ ] cycle the selection, K exports CSV) ---
	// Same shape as the debug-overlay state above: the pawn writes, the Slate panel reads.
	void ToggleAnalytics() { bAnalyticsOpen = !bAnalyticsOpen; }
	bool IsAnalyticsOpen() const { return bAnalyticsOpen; }

	/** Next / previous page (wraps). */
	void CycleAnalyticsPage(int32 Direction);
	EAnalyticsPage GetAnalyticsPage() const { return AnalyticsPage; }

	/** Cycles whatever the current page selects: the charted trait, or the scatter axis pair. */
	void CycleAnalyticsSelection(int32 Direction);
	EBoidStat GetAnalyticsTrait() const { return static_cast<EBoidStat>(AnalyticsTraitIndex); }
	const FScatterPreset& GetAnalyticsScatterPreset() const { return Evo::ScatterPresets[AnalyticsScatterIndex]; }

	/** Dumps the recorded run to Saved/Evoswarm/ and reports the outcome in the event feed. */
	void ExportAnalyticsCsv();

	// --- Spawning (safe: invoked outside Mass processing) ---
	FMassEntityHandle SpawnBoid(const FBoidSpeciesSharedFragment& Shared, const FBoidGenome& Genome, const FVector& Position, int32 Generation = 0);
	FMassEntityHandle SpawnFood(const FVector& Position, EFoodType Type = EFoodType::Plant, float Energy = -1.f);

	// --- Called from processors during the frame (counter / queue only, no structural change) ---
	void RequestBirth(const FBoidSpeciesSharedFragment& Shared, const FBoidGenome& Genome, const FVector& Position, int32 ChildGeneration);
	void RequestCarcass(const FVector& Position, float Energy);
	/**
	 * A plant was grazed. Energy is post-digestion (what the eater actually gained), so the
	 * food-web arrows reflect nutrition rather than bite count.
	 */
	void NotifyPlantEaten(int32 SpeciesIndex, float EnergyGained);

	/** A bite was taken out of a carcass. Energy is post-digestion, as above. */
	void NotifyMeatEaten(int32 SpeciesIndex, float EnergyGained);

	// --- Ecosystem event counters (all boid processors run on the game thread, so plain ints) ---
	/** Record a non-predation death (starvation / old age / combat wounds). */
	void NotifyDeath(int32 SpeciesIndex, EDeathCause Cause);
	/** Record a successful hunt: a kill for the killer AND a predation death for the victim. */
	void NotifyKill(int32 KillerSpeciesIndex, int32 VictimSpeciesIndex);

	// --- Event feed (extinctions, generation milestones, diet shifts, collapses) ---
	const TArray<FSimEvent>& GetEventLog() const { return EventLog; }
	void LogEvent(const FString& Text, const FLinearColor& Color);

	float GetElapsedTime() const { return ElapsedTime; }

	// --- Crosshair inspect (pawn writes, HUD reads) ---
	const FBoidInspectState& GetInspect() const { return Inspect; }
	FBoidInspectState& GetInspectMutable() { return Inspect; }

	// --- UTickableWorldSubsystem ---
	virtual void Tick(float DeltaTime) override;
	virtual void Deinitialize() override;
	virtual bool IsTickable() const override { return bRunning; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UEvoswarmSimSubsystem, STATGROUP_Tickables); }

private:
	FMassEntityManager* GetEntityManager() const;
	void FlushBirths();
	void FlushCarcasses();
	void RegrowFood();
	FVector RandomArenaPoint();

	// --- Reproduction feedback: a quick expanding ring + a soft, throttled blip ---
	void OnReproduction(const FVector& Position);
	void DrawBirthFlashes(float DeltaTime);
	void PlayBirthBlip();

	struct FBirthFlash { FVector Position = FVector::ZeroVector; float Age = 0.f; };
	TArray<FBirthFlash> BirthFlashes;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BirthAudio;
	UPROPERTY(Transient)
	TObjectPtr<USoundWaveProcedural> BirthSound;
	float LastBirthSfxTime = -10.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> BoidBucketISM;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> FoodISM;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> CarcassISM;

	TArray<FBoidBirthRequest> PendingBirths;
	TArray<TPair<FVector, float>> PendingCarcasses;
	TArray<FSpeciesLiveStats> SpeciesStats;
	TArray<FSimEvent> EventLog;
	void SampleHistory();
	void DetectEvents();

	// --- Accélérateur de temps ---
	/** Pousse le palier courant dans le monde et gère l'auto-régulation. RealDelta = frame non dilatée. */
	void UpdateTimeScale(float RealDelta);
	/** Allège (ou restaure) le rendu au-dessus de Evo::TurboSpeedThreshold. */
	void ApplyTurbo(bool bOn);

	int32 SpeedIndex = Evo::DefaultSpeedIndex;
	bool bPaused = false;
	bool bTurboActive = false;
	float EffectiveSpeed = 1.f;
	float LastSimStep = 0.f;
	float RealFpsEma = 0.f;
	float ThrottleCooldown = 0.f;
	/** Valeurs des CVars de rendu avant passage en turbo, pour les remettre à l'identique. */
	TMap<FString, float> TurboSavedCVars;

	FRandomStream Rng = FRandomStream(1337);
	int32 FoodCount = 0;
	int32 LivePlantCount = 0;
	int32 LiveCarcassCount = 0;
	bool bRunning = false;
	bool bDebugDraw = false;
	int32 CurrentDebugMode = 0;

	/** Row-major [Killer * NumSpecies + Victim]; grown lazily as species register. */
	TArray<int32> KillMatrix;
	void EnsureKillMatrixSize();

	bool bAnalyticsOpen = false;
	EAnalyticsPage AnalyticsPage = EAnalyticsPage::TraitCurves;
	int32 AnalyticsTraitIndex = static_cast<int32>(EBoidStat::Diet); // diet drift is the best opener
	int32 AnalyticsScatterIndex = 0;

	float ElapsedTime = 0.f;       // seconds since the sim started
	float HistoryTimer = 0.f;      // accumulator for population sampling

	/** Sim-time stamps at which an eaten plant's replacement is allowed to sprout. */
	TArray<float> PendingRegrowth;

	/** Fertile meadow patch: plants regrow in bunches around these, and they slowly relocate. */
	struct FFoodPatch
	{
		FVector Center = FVector::ZeroVector;
		float Expiry = -1.f;
	};
	TArray<FFoodPatch> FoodPatches;
	void EnsureFoodPatches();

	// --- Performance logging (Saved/Logs/EvoPerf.csv + LogEvoPerf; disable with -EvoNoPerfLog) ---
	void PerfLogTick(float DeltaTime);
	void OpenPerfLog();
	class FArchive* PerfLogFile = nullptr;
	bool bPerfLogEnabled = true;
	bool bPerfLogOpened = false;
	float PerfLogTimer = 0.f;
	float PerfFrameMsAccum = 0.f;
	float PerfFrameMsPeak = 0.f;
	int32 PerfFrameCount = 0;

	FBoidInspectState Inspect;
};