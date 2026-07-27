// Copyright Evoswarm.

#include "EvoswarmAnalytics.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// -------------------------------------------------------------------------------------------
// Stat labels
// -------------------------------------------------------------------------------------------
const TCHAR* Evo::StatShortName(EBoidStat Stat)
{
	switch (Stat)
	{
	case EBoidStat::HP:               return TEXT("HP");
	case EBoidStat::Armor:            return TEXT("ARM");
	case EBoidStat::WalkSpeed:        return TEXT("WALK");
	case EBoidStat::RunSpeed:         return TEXT("RUN");
	case EBoidStat::Stamina:          return TEXT("STAM");
	case EBoidStat::Regeneration:     return TEXT("REGEN");
	case EBoidStat::Hunger:           return TEXT("HUNG");
	case EBoidStat::Biomass:          return TEXT("BIO");
	case EBoidStat::Stealth:          return TEXT("STLH");
	case EBoidStat::Damage:           return TEXT("DMG");
	case EBoidStat::Intimidation:     return TEXT("INTIM");
	case EBoidStat::Aggressiveness:   return TEXT("AGGR");
	case EBoidStat::Perception:       return TEXT("PERC");
	case EBoidStat::Diet:             return TEXT("DIET");
	case EBoidStat::Lifespan:         return TEXT("LIFE");
	case EBoidStat::ReproductionRate: return TEXT("REPRO");
	case EBoidStat::Integration:      return TEXT("INTEG");
	case EBoidStat::MutationRate:     return TEXT("MUT");
	default:                          return TEXT("?");
	}
}

const TCHAR* Evo::StatLongName(EBoidStat Stat)
{
	switch (Stat)
	{
	case EBoidStat::HP:               return TEXT("Hit points");
	case EBoidStat::Armor:            return TEXT("Armour");
	case EBoidStat::WalkSpeed:        return TEXT("Walk speed");
	case EBoidStat::RunSpeed:         return TEXT("Run speed");
	case EBoidStat::Stamina:          return TEXT("Stamina");
	case EBoidStat::Regeneration:     return TEXT("Regeneration");
	case EBoidStat::Hunger:           return TEXT("Hunger capacity");
	case EBoidStat::Biomass:          return TEXT("Biomass");
	case EBoidStat::Stealth:          return TEXT("Stealth");
	case EBoidStat::Damage:           return TEXT("Damage");
	case EBoidStat::Intimidation:     return TEXT("Intimidation");
	case EBoidStat::Aggressiveness:   return TEXT("Aggressiveness");
	case EBoidStat::Perception:       return TEXT("Perception");
	case EBoidStat::Diet:             return TEXT("Diet (herbivore to carnivore)");
	case EBoidStat::Lifespan:         return TEXT("Lifespan");
	case EBoidStat::ReproductionRate: return TEXT("Reproduction rate");
	case EBoidStat::Integration:      return TEXT("Integration (flocking)");
	case EBoidStat::MutationRate:     return TEXT("Mutation rate");
	default:                          return TEXT("Unknown");
	}
}

// -------------------------------------------------------------------------------------------
// FSpeciesTimeline
// -------------------------------------------------------------------------------------------
void FSpeciesTimeline::Push(const FSpeciesTimeSample& In)
{
	if (Stride <= 1)
	{
		Samples.Add(In);
	}
	else
	{
		// Fold this arrival into the pending group; only emit once the group is full, so the
		// stored spacing stays exactly Interval.
		Pending.Accumulate(In, PendingCount);
		++PendingCount;
		if (PendingCount < Stride)
		{
			return;
		}
		Samples.Add(Pending);
		Pending = FSpeciesTimeSample();
		PendingCount = 0;
	}

	if (Samples.Num() >= Evo::TimelineMaxSamples)
	{
		Decimate();
	}
}

void FSpeciesTimeline::Decimate()
{
	// Merge adjacent pairs in place: [0,1] -> [0], [2,3] -> [1], ...
	const int32 N = Samples.Num();
	const int32 NewNum = N / 2;
	for (int32 I = 0; I < NewNum; ++I)
	{
		Samples[I] = Samples[I * 2];
		Samples[I].MergeEqual(Samples[I * 2 + 1]);
	}
	// An odd tail sample would sit at half spacing from its neighbour and break the uniform
	// grid, so carry it in Pending instead of storing it.
	const bool bHasTail = (N & 1) != 0;
	if (bHasTail)
	{
		Pending = Samples[N - 1];
	}
	Samples.SetNum(NewNum, EAllowShrinking::No);

	Stride *= 2;
	Interval *= 2.f;
	// The carried tail already represents Stride/2 raw arrivals at the new stride.
	PendingCount = bHasTail ? (Stride / 2) : 0;
}

void FSpeciesTimeline::Reset()
{
	Samples.Reset();
	Pending = FSpeciesTimeSample();
	PendingCount = 0;
	Stride = 1;
	Interval = Evo::StatsSampleInterval;
}

// -------------------------------------------------------------------------------------------
// CSV export
// -------------------------------------------------------------------------------------------
namespace
{
	/** Quote a field only if it needs it, so the common case stays readable in a text editor. */
	FString CsvField(const FString& In)
	{
		if (In.Contains(TEXT(",")) || In.Contains(TEXT("\"")) || In.Contains(TEXT("\n")))
		{
			return FString::Printf(TEXT("\"%s\""), *In.Replace(TEXT("\""), TEXT("\"\"")));
		}
		return In;
	}
}

FString Evo::ExportRunToCsv(const TArray<FCsvSpeciesInput>& Species, const TArray<int32>& KillMatrix, FString& OutError)
{
	OutError.Reset();

	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Evoswarm");
	if (!IFileManager::Get().DirectoryExists(*Dir) && !IFileManager::Get().MakeDirectory(*Dir, true))
	{
		OutError = FString::Printf(TEXT("could not create %s"), *Dir);
		return FString();
	}

	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));

	// --- Traits: one row per (species, sample) -------------------------------------------
	{
		FString Csv;
		Csv.Reserve(1 << 20);

		Csv += TEXT("time_s,species,population,births_per_min,deaths_per_min,avg_generation");
		for (int32 S = 0; S < NumBoidStats; ++S)
		{
			const TCHAR* Short = StatShortName(static_cast<EBoidStat>(S));
			Csv += FString::Printf(TEXT(",%s_mean,%s_sd,%s_min,%s_max"), Short, Short, Short, Short);
		}
		Csv += LINE_TERMINATOR;

		for (const FCsvSpeciesInput& Sp : Species)
		{
			if (!Sp.Timeline)
			{
				continue;
			}
			const FString Name = CsvField(Sp.Name);
			for (const FSpeciesTimeSample& Sample : Sp.Timeline->Samples)
			{
				Csv += FString::Printf(TEXT("%.2f,%s,%.1f,%.2f,%.2f,%.2f"),
					Sample.Time, *Name, Sample.Count, Sample.BirthRate, Sample.DeathRate, Sample.AvgGeneration);
				for (int32 S = 0; S < NumBoidStats; ++S)
				{
					const FTraitDistribution& T = Sample.Traits[S];
					Csv += FString::Printf(TEXT(",%.4f,%.4f,%.4f,%.4f"), T.Mean, T.StdDev, T.Min, T.Max);
				}
				Csv += LINE_TERMINATOR;
			}
		}

		const FString Path = Dir / FString::Printf(TEXT("evoswarm_traits_%s.csv"), *Stamp);
		if (!FFileHelper::SaveStringToFile(Csv, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("could not write %s"), *Path);
			return FString();
		}
	}

	// --- Trophic: intake totals + the kill matrix ----------------------------------------
	{
		const int32 N = Species.Num();
		FString Csv;

		Csv += TEXT("species,plant_energy_total,meat_energy_total,plants_eaten,carcasses_dropped");
		for (int32 V = 0; V < N; ++V)
		{
			Csv += FString::Printf(TEXT(",kills_of_%s"), *CsvField(Species[V].Name));
		}
		Csv += LINE_TERMINATOR;

		for (int32 K = 0; K < N; ++K)
		{
			const FTrophicLedger* L = Species[K].Trophic;
			Csv += FString::Printf(TEXT("%s,%.2f,%.2f,%d,%d"),
				*CsvField(Species[K].Name),
				L ? L->PlantEnergy : 0.f,
				L ? L->MeatEnergy : 0.f,
				L ? L->PlantsEaten : 0,
				L ? L->CarcassesDropped : 0);
			for (int32 V = 0; V < N; ++V)
			{
				const int32 Index = K * N + V;
				Csv += FString::Printf(TEXT(",%d"), KillMatrix.IsValidIndex(Index) ? KillMatrix[Index] : 0);
			}
			Csv += LINE_TERMINATOR;
		}

		const FString Path = Dir / FString::Printf(TEXT("evoswarm_trophic_%s.csv"), *Stamp);
		if (!FFileHelper::SaveStringToFile(Csv, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			OutError = FString::Printf(TEXT("could not write %s"), *Path);
			return FString();
		}
	}

	return Dir;
}