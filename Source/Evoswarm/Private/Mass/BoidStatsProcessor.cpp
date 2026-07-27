// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"
#include "EvoswarmSimSubsystem.h"

UBoidStatsProcessor::UBoidStatsProcessor()
	: EntityQuery(*this)
	, FoodQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidReproductionProcessor"));
}

void UBoidStatsProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSharedRequirement<FBoidSpeciesSharedFragment>(EMassFragmentAccess::ReadOnly);

	// Display-only census of the food field, split by type, for the food-web page. Counted here
	// rather than hooked into feeding + decay so exactly one place has to get it right.
	FoodQuery.AddTagRequirement<FFoodTag>(EMassFragmentPresence::All);
	FoodQuery.AddRequirement<FFoodFragment>(EMassFragmentAccess::ReadOnly);
}

void UBoidStatsProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	if (!Sim || Sim->NumSpecies() == 0)
	{
		return;
	}

	TArray<FSpeciesLiveStats>& Stats = Sim->GetSpeciesStatsMutable();
	const int32 N = Stats.Num();

	// Per-stat accumulators. Carrying the sum of squares (rather than making a second pass over
	// the entities) yields the standard deviation from the same single sweep:
	//     sd = sqrt(E[x^2] - E[x]^2)
	// Doubles, because SumSq over ~1600 entities loses too much in float.
	struct FStatAccum
	{
		double Sum = 0.0;
		double SumSq = 0.0;
		float Min = TNumericLimits<float>::Max();
		float Max = TNumericLimits<float>::Lowest();
	};
	TArray<FStatAccum> Accum;
	Accum.SetNum(N * NumBoidStats);

	// Stride for the scatter subsample. It has to be decided before walking the chunks, so it
	// uses last frame's population - one frame stale is irrelevant for choosing a step size.
	TArray<int32> ScatterStride;
	ScatterStride.SetNumUninitialized(N);

	// Reset accumulators (preserve Name/Color which the game mode set, and the lifetime
	// counters / histories / timeline which the subsystem owns).
	for (int32 I = 0; I < N; ++I)
	{
		FSpeciesLiveStats& S = Stats[I];
		// Rounded UP: with 400 alive and a cap of 260, a stride of 1 would take the first 260
		// and stop, biasing the cloud towards whichever chunks came first. Stride 2 takes 200
		// spread evenly across the whole population instead.
		ScatterStride[I] = FMath::Max(1, FMath::DivideAndRoundUp(S.Count, Evo::ScatterMaxPoints));

		S.Count = 0;
		S.AvgHP = S.AvgWalkSpeed = S.AvgPerception = S.AvgDiet = 0.f;
		S.AvgLifespan = S.AvgDamage = S.AvgArmor = S.AvgMutationRate = 0.f;
		S.AvgGeneration = 0.f;
		S.MaxGeneration = 0;
		S.GenomeSamples.Reset();
		S.GenomeSamples.Reserve(Evo::ScatterMaxPoints);
		// One bin per render diet hue, so the HUD histogram and creature colours line up.
		// (SetNumZeroed only zeroes NEW elements, so clear explicitly every frame.)
		S.DietHistogram.SetNumZeroed(Evo::NumDietHues);
		for (int32& Bin : S.DietHistogram)
		{
			Bin = 0;
		}
	}

	// Individuals seen per species this frame, so the scatter stride keeps stepping across
	// chunk boundaries instead of restarting in each chunk.
	TArray<int32> Seen;
	Seen.SetNumZeroed(N);

	EntityQuery.ForEachEntityChunk(Context, [&Stats, &Accum, &Seen, &ScatterStride, N](FMassExecutionContext& Context)
		{
			const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
			if (Species.SpeciesIndex < 0 || Species.SpeciesIndex >= N)
			{
				return;
			}
			const int32 Si = Species.SpeciesIndex;
			FSpeciesLiveStats& S = Stats[Si];
			FStatAccum* A = &Accum[Si * NumBoidStats];
			const int32 Stride = ScatterStride[Si];

			const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
			const TConstArrayView<FBoidStateFragment> StateView = Context.GetFragmentView<FBoidStateFragment>();
			for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
			{
				const FBoidGenome& G = Gen[It].Genome;
				++S.Count;
				++S.DietHistogram[Evo::HueBucket(G.Get(EBoidStat::Diet))]; // same binning as the render hues

				for (int32 St = 0; St < NumBoidStats; ++St)
				{
					const float V = G.Stats[St];
					FStatAccum& Acc = A[St];
					Acc.Sum += V;
					Acc.SumSq += static_cast<double>(V) * static_cast<double>(V);
					Acc.Min = FMath::Min(Acc.Min, V);
					Acc.Max = FMath::Max(Acc.Max, V);
				}

				// Keep every Nth individual whole, for the scatter page.
				if ((Seen[Si] % Stride) == 0 && S.GenomeSamples.Num() < Evo::ScatterMaxPoints)
				{
					S.GenomeSamples.Add(G);
				}
				++Seen[Si];

				const int32 Gens = StateView[It].Generation;
				S.AvgGeneration += static_cast<float>(Gens);
				S.MaxGeneration = FMath::Max(S.MaxGeneration, Gens);
			}
		});

	// Finalise: sums -> distributions.
	for (int32 I = 0; I < N; ++I)
	{
		FSpeciesLiveStats& S = Stats[I];
		const FStatAccum* A = &Accum[I * NumBoidStats];

		if (S.Count > 0)
		{
			const double Inv = 1.0 / static_cast<double>(S.Count);
			for (int32 St = 0; St < NumBoidStats; ++St)
			{
				const double Mean = A[St].Sum * Inv;
				const double Variance = FMath::Max(0.0, A[St].SumSq * Inv - Mean * Mean);
				FTraitDistribution& D = S.TraitNow[St];
				D.Mean = static_cast<float>(Mean);
				D.StdDev = static_cast<float>(FMath::Sqrt(Variance));
				D.Min = A[St].Min;
				D.Max = A[St].Max;
			}
			S.AvgGeneration *= static_cast<float>(Inv);
		}
		else
		{
			// Extinct: report zeroes, not the sentinels, so the charts read as a flat line at
			// zero rather than a spike to FLT_MAX.
			for (int32 St = 0; St < NumBoidStats; ++St)
			{
				S.TraitNow[St] = FTraitDistribution();
			}
		}

		// The named averages the existing panel rows read by name are simply the means.
		S.AvgHP = S.TraitNow[StatIndex(EBoidStat::HP)].Mean;
		S.AvgWalkSpeed = S.TraitNow[StatIndex(EBoidStat::WalkSpeed)].Mean;
		S.AvgPerception = S.TraitNow[StatIndex(EBoidStat::Perception)].Mean;
		S.AvgDiet = S.TraitNow[StatIndex(EBoidStat::Diet)].Mean;
		S.AvgLifespan = S.TraitNow[StatIndex(EBoidStat::Lifespan)].Mean;
		S.AvgDamage = S.TraitNow[StatIndex(EBoidStat::Damage)].Mean;
		S.AvgArmor = S.TraitNow[StatIndex(EBoidStat::Armor)].Mean;
		S.AvgMutationRate = S.TraitNow[StatIndex(EBoidStat::MutationRate)].Mean;
	}

	// Food census: plants vs carcasses currently standing in the world.
	int32 Plants = 0;
	int32 Carcasses = 0;
	FoodQuery.ForEachEntityChunk(Context, [&Plants, &Carcasses](FMassExecutionContext& Context)
		{
			const TConstArrayView<FFoodFragment> Food = Context.GetFragmentView<FFoodFragment>();
			for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
			{
				if (Food[It].Type == EFoodType::Carcass)
				{
					++Carcasses;
				}
				else
				{
					++Plants;
				}
			}
		});
	Sim->SetFoodCensus(Plants, Carcasses);
}