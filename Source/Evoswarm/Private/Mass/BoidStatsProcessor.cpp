// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmSimSubsystem.h"

UBoidStatsProcessor::UBoidStatsProcessor()
	: EntityQuery(*this)
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

	// Reset accumulators (preserve Name/Color which the game mode set).
	for (FSpeciesLiveStats& S : Stats)
	{
		S.Count = 0;
		S.AvgHP = S.AvgWalkSpeed = S.AvgPerception = S.AvgDiet = 0.f;
		S.AvgLifespan = S.AvgDamage = S.AvgArmor = S.AvgMutationRate = 0.f;
		S.AvgGeneration = 0.f;
		S.MaxGeneration = 0;
	}

	EntityQuery.ForEachEntityChunk(Context, [&Stats, N](FMassExecutionContext& Context)
	{
		const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
		if (Species.SpeciesIndex < 0 || Species.SpeciesIndex >= N)
		{
			return;
		}
		FSpeciesLiveStats& S = Stats[Species.SpeciesIndex];
		const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
		const TConstArrayView<FBoidStateFragment> StateView = Context.GetFragmentView<FBoidStateFragment>();
		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			const FBoidGenome& G = Gen[It].Genome;
			++S.Count;
			S.AvgHP            += G.Get(EBoidStat::HP);
			S.AvgWalkSpeed     += G.Get(EBoidStat::WalkSpeed);
			S.AvgPerception    += G.Get(EBoidStat::Perception);
			S.AvgDiet          += G.Get(EBoidStat::Diet);
			S.AvgLifespan      += G.Get(EBoidStat::Lifespan);
			S.AvgDamage        += G.Get(EBoidStat::Damage);
			S.AvgArmor         += G.Get(EBoidStat::Armor);
			S.AvgMutationRate  += G.Get(EBoidStat::MutationRate);
			const int32 Gens = StateView[It].Generation;
			S.AvgGeneration    += static_cast<float>(Gens);
			S.MaxGeneration = FMath::Max(S.MaxGeneration, Gens);
		}
	});

	// Finalise: sums -> averages.
	for (FSpeciesLiveStats& S : Stats)
	{
		if (S.Count > 0)
		{
			const float Inv = 1.f / static_cast<float>(S.Count);
			S.AvgHP *= Inv;
			S.AvgWalkSpeed *= Inv;
			S.AvgPerception *= Inv;
			S.AvgDiet *= Inv;
			S.AvgLifespan *= Inv;
			S.AvgDamage *= Inv;
			S.AvgArmor *= Inv;
			S.AvgMutationRate *= Inv;
			S.AvgGeneration *= Inv;
		}
	}
}
