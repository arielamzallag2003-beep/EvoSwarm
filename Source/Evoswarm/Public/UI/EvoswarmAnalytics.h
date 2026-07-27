// Copyright Evoswarm.
//
// The analytics data layer behind the G overlay. Three concerns live here:
//
//  1. FTraitDistribution / FSpeciesTimeSample - what we record about a population at one
//     instant. Note that we keep the SPREAD (sd, min, max), not just the mean: a mean of
//     0.5 on Diet reads "omnivore" whether the population is uniformly omnivorous or has
//     split into a herbivore clade and a carnivore clade. Evolution lives in the spread.
//
//  2. FSpeciesTimeline - a uniformly-spaced series covering the WHOLE run in bounded
//     memory, via progressive decimation (see the comment on the struct).
//
//  3. FTrophicLedger - who eats what, so the food-web page can size its arrows by real
//     energy throughput instead of drawing a fixed diagram.
//
// Nothing here knows about Slate or about the sim subsystem, so it stays testable and
// include-cheap; the subsystem owns the instances and the widgets only read them.

#pragma once

#include "CoreMinimal.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"

// -------------------------------------------------------------------------------------------
// Stat labels
// -------------------------------------------------------------------------------------------
namespace Evo
{
	/** Axis-sized label (<= 5 chars). Chart gutters have no room for "Taux de Reproduction". */
	EVOSWARM_API const TCHAR* StatShortName(EBoidStat Stat);

	/** Readable label for the page header and CSV column names. */
	EVOSWARM_API const TCHAR* StatLongName(EBoidStat Stat);
}

// -------------------------------------------------------------------------------------------
// One stat's distribution across a population
// -------------------------------------------------------------------------------------------
struct FTraitDistribution
{
	float Mean = 0.f;
	float StdDev = 0.f;
	float Min = 0.f;
	float Max = 0.f;

	/** Merge two equally-weighted samples (used when the timeline decimates). */
	void MergeEqual(const FTraitDistribution& Other)
	{
		Mean = 0.5f * (Mean + Other.Mean);
		// Averaging the sds understates the merged sd slightly (it ignores the drift of the
		// means between the two samples). At the scale of one decimation step that error is
		// far below chart resolution, and it keeps the merge O(1) with no extra stored moments.
		StdDev = 0.5f * (StdDev + Other.StdDev);
		Min = FMath::Min(Min, Other.Min);
		Max = FMath::Max(Max, Other.Max);
	}

	/** Fold a new sample into a running average of NumSoFar previous samples. */
	void Accumulate(const FTraitDistribution& In, int32 NumSoFar)
	{
		const float W = 1.f / static_cast<float>(NumSoFar + 1);
		Mean = (Mean * NumSoFar + In.Mean) * W;
		StdDev = (StdDev * NumSoFar + In.StdDev) * W;
		Min = (NumSoFar == 0) ? In.Min : FMath::Min(Min, In.Min);
		Max = (NumSoFar == 0) ? In.Max : FMath::Max(Max, In.Max);
	}
};

// -------------------------------------------------------------------------------------------
// One time point of one species' recorded history
// -------------------------------------------------------------------------------------------
struct FSpeciesTimeSample
{
	float Time = 0.f;            // sim seconds at the END of the interval this sample covers
	float Count = 0.f;           // population (float so decimation can average it)
	float BirthRate = 0.f;       // births per minute over the sampling window
	float DeathRate = 0.f;       // deaths per minute over the sampling window
	float AvgGeneration = 0.f;
	FTraitDistribution Traits[NumBoidStats];

	void MergeEqual(const FSpeciesTimeSample& Other)
	{
		Time = FMath::Max(Time, Other.Time);   // the merged point represents "as of" the later edge
		Count = 0.5f * (Count + Other.Count);
		BirthRate = 0.5f * (BirthRate + Other.BirthRate);
		DeathRate = 0.5f * (DeathRate + Other.DeathRate);
		AvgGeneration = 0.5f * (AvgGeneration + Other.AvgGeneration);
		for (int32 I = 0; I < NumBoidStats; ++I)
		{
			Traits[I].MergeEqual(Other.Traits[I]);
		}
	}

	void Accumulate(const FSpeciesTimeSample& In, int32 NumSoFar)
	{
		const float W = 1.f / static_cast<float>(NumSoFar + 1);
		Time = FMath::Max(Time, In.Time);
		Count = (Count * NumSoFar + In.Count) * W;
		BirthRate = (BirthRate * NumSoFar + In.BirthRate) * W;
		DeathRate = (DeathRate * NumSoFar + In.DeathRate) * W;
		AvgGeneration = (AvgGeneration * NumSoFar + In.AvgGeneration) * W;
		for (int32 I = 0; I < NumBoidStats; ++I)
		{
			Traits[I].Accumulate(In.Traits[I], NumSoFar);
		}
	}
};

// -------------------------------------------------------------------------------------------
// Whole-run history in bounded memory
// -------------------------------------------------------------------------------------------
/**
 * A uniformly-spaced series that never drops the start of the run and never grows past
 * Evo::TimelineMaxSamples entries.
 *
 * Samples arrive every Evo::StatsSampleInterval seconds. While the buffer has room each
 * arrival is stored as-is. When it fills, Decimate() merges every adjacent PAIR in place:
 * the array halves, the spacing doubles, and from then on two arrivals are folded together
 * before being appended. Repeat forever. So a 4-hour run and a 4-minute run both occupy the
 * same memory; the only thing that degrades is the time resolution of the older part, which
 * is exactly what you can afford to lose when the X axis is "the whole run".
 *
 * Spacing stays uniform across the entire buffer, so widgets can index it as a plain array
 * and CSV rows are evenly spaced.
 */
struct FSpeciesTimeline
{
	/** Oldest first, spaced Interval seconds apart. */
	TArray<FSpeciesTimeSample> Samples;

	/** Current spacing between stored samples, in seconds. Grows by 2x on each decimation. */
	float Interval = Evo::StatsSampleInterval;

	void Push(const FSpeciesTimeSample& In);
	void Reset();

	int32 Num() const { return Samples.Num(); }
	bool IsEmpty() const { return Samples.Num() == 0; }

	/** Seconds spanned by the stored samples (0 when fewer than two). */
	float SpanSeconds() const { return (Samples.Num() < 2) ? 0.f : (Samples.Last().Time - Samples[0].Time); }

private:
	void Decimate();

	FSpeciesTimeSample Pending;   // accumulator, active once Stride > 1
	int32 PendingCount = 0;
	int32 Stride = 1;             // raw arrivals folded into one stored sample
};

// -------------------------------------------------------------------------------------------
// Trophic ledger (food-web page)
// -------------------------------------------------------------------------------------------
/**
 * Energy actually moved between trophic levels, per species. Amounts are in hunger units
 * (post-digestion, i.e. what the eater really gained), so a carnivore eating a plant it
 * digests badly contributes little - the arrow widths reflect biology, not bite counts.
 */
struct FTrophicLedger
{
	// --- Cumulative since sim start ---
	float PlantEnergy = 0.f;      // gained from grazing plants
	float MeatEnergy = 0.f;       // gained from carcasses (own kills + scavenging)
	int32 PlantsEaten = 0;
	int32 CarcassesDropped = 0;   // carcasses this species' deaths produced

	// --- Per-sample deltas, aligned with the other histories (oldest first) ---
	TArray<float> PlantEnergyHistory;
	TArray<float> MeatEnergyHistory;

	// Bookkeeping for the deltas above.
	float LastSampledPlantEnergy = 0.f;
	float LastSampledMeatEnergy = 0.f;

	void Reset()
	{
		*this = FTrophicLedger();
	}
};

namespace Evo
{
	/** Float twin of RatePerMinute (which is int32-only), for the energy-flow histories. */
	inline float RatePerMinuteF(const TArray<float>& History, float WindowSec = FlowRateWindowSec)
	{
		const int32 N = History.Num();
		if (N == 0)
		{
			return 0.f;
		}
		const int32 WindowSamples = FMath::Max(1, FMath::RoundToInt(WindowSec / StatsSampleInterval));
		const int32 Use = FMath::Min(WindowSamples, N);
		float Sum = 0.f;
		for (int32 I = N - Use; I < N; ++I)
		{
			Sum += History[I];
		}
		const float Seconds = Use * StatsSampleInterval;
		return (Seconds > 0.f) ? (Sum * 60.f / Seconds) : 0.f;
	}
}

// -------------------------------------------------------------------------------------------
// Overlay pages + scatter axis presets
// -------------------------------------------------------------------------------------------
enum class EAnalyticsPage : uint8
{
	TraitCurves,     // one trait, every species, mean +/- 1 sd, whole run
	Populations,     // stacked population area + birth/death rates
	Scatter,         // individual genomes in 2D trait space (shows clades separating)
	FoodWeb,         // who eats what, arrows sized by energy throughput

	Count
};
static constexpr int32 NumAnalyticsPages = static_cast<int32>(EAnalyticsPage::Count);

/** A curated pair of axes for the scatter page, with a one-line reading of what it shows. */
struct FScatterPreset
{
	EBoidStat X;
	EBoidStat Y;
	const TCHAR* Story;
};

namespace Evo
{
	/**
	 * Cycled with [ and ]. These pairs are chosen because a trade-off is *expected* along
	 * them, so a population splitting into two strategies shows up as two clouds rather
	 * than one blob - the r/K pair (Lifespan x ReproductionRate) is the classic example.
	 */
	inline const FScatterPreset ScatterPresets[] =
	{
		{ EBoidStat::RunSpeed,     EBoidStat::Perception,       TEXT("outrun it or see it coming") },
		{ EBoidStat::Lifespan,     EBoidStat::ReproductionRate, TEXT("live long or breed fast (r/K)") },
		{ EBoidStat::Damage,       EBoidStat::Armor,            TEXT("offence or defence") },
		{ EBoidStat::HP,           EBoidStat::RunSpeed,         TEXT("bulk or speed") },
		{ EBoidStat::Diet,         EBoidStat::Perception,        TEXT("grazing or hunting senses") },
		{ EBoidStat::Stealth,      EBoidStat::Intimidation,     TEXT("ambush or threat") },
		{ EBoidStat::Biomass,      EBoidStat::Damage,           TEXT("prey value vs predator power") },
		{ EBoidStat::MutationRate, EBoidStat::Diet,             TEXT("who is still experimenting") },
	};
	inline constexpr int32 NumScatterPresets = UE_ARRAY_COUNT(ScatterPresets);
}

// -------------------------------------------------------------------------------------------
// CSV export
// -------------------------------------------------------------------------------------------
namespace Evo
{
	/** One species' data as handed to the exporter (avoids a dependency on the subsystem types). */
	struct FCsvSpeciesInput
	{
		FString Name;
		const FSpeciesTimeline* Timeline = nullptr;
		const FTrophicLedger* Trophic = nullptr;
	};

	/**
	 * Writes two files under Saved/Evoswarm/:
	 *   evoswarm_traits_<stamp>.csv   - one row per (species, time sample), every stat's mean/sd/min/max
	 *   evoswarm_trophic_<stamp>.csv  - the kill matrix and per-species energy intake totals
	 *
	 * KillMatrix is row-major [Killer * NumSpecies + Victim]. Returns the directory written to,
	 * or an empty string on failure (with the reason in OutError).
	 */
	EVOSWARM_API FString ExportRunToCsv(
		const TArray<FCsvSpeciesInput>& Species,
		const TArray<int32>& KillMatrix,
		FString& OutError);
}