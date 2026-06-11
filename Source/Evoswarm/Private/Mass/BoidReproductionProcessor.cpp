// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "SpeciesConfig.h"
#include "EvoswarmTuning.h"
#include "EvoswarmSimSubsystem.h"
#include "BoidGridSubsystem.h"

UBoidReproductionProcessor::UBoidReproductionProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidMetabolismProcessor"));
}

void UBoidReproductionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSharedRequirement<FBoidSpeciesSharedFragment>(EMassFragmentAccess::ReadOnly);
}

void UBoidReproductionProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	UBoidGridSubsystem* Grid = World ? World->GetSubsystem<UBoidGridSubsystem>() : nullptr;
	if (!Sim || !Grid)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager, Sim, Grid](FMassExecutionContext& Context)
	{
		const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
		const USpeciesConfig* Config = Species.Config;
		if (!Config)
		{
			return;
		}

		// Carrying capacity: once a species hits its cap, it stops breeding.
		if (Sim->GetSpeciesCount(Species.SpeciesIndex) >= Config->MaxPopulation)
		{
			return;
		}

		const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
		const TArrayView<FBoidStateFragment> State = Context.GetMutableFragmentView<FBoidStateFragment>();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			FBoidStateFragment& S = State[It];
			const FBoidGenome& G = Gen[It].Genome;
			const float MaxHunger = Evo::MaxHunger(G);

			// Must be a mature, rested, well-fed individual to initiate breeding.
			if (S.Age < Evo::MaturityAge || S.ReproCooldown > 0.f || S.CurrentHunger < Evo::ReproHungerFraction * MaxHunger)
			{
				continue;
			}

			const FMassEntityHandle Self = Context.GetEntity(It);
			if (!Grid->TryClaim(Self)) // already paired by someone this frame
			{
				continue;
			}
			const FVector Pos = Xf[It].GetTransform().GetLocation();

			// Sexual selection: among ready same-species partners in range, choose the one in
			// best body condition (the fittest survivor), not merely the closest one.
			FMassEntityHandle Mate;
			FVector MatePos = FVector::ZeroVector;
			float BestCondition = -1.f;
			bool bFoundMate = false;
			Grid->QueryAgents(Pos, Evo::MatingRadius, [&](const FGridAgent& Other)
			{
				if (Other.SpeciesIndex != Species.SpeciesIndex || !Other.bCanMate || Other.Entity == Self)
				{
					return;
				}
				if (Other.Condition > BestCondition)
				{
					BestCondition = Other.Condition;
					Mate = Other.Entity;
					MatePos = Other.Position;
					bFoundMate = true;
				}
			});

			if (!bFoundMate || !Grid->TryClaim(Mate))
			{
				continue; // no available partner this frame
			}

			FBoidGenomeFragment* MateGen = EntityManager.GetFragmentDataPtr<FBoidGenomeFragment>(Mate);
			FBoidStateFragment* MateState = EntityManager.GetFragmentDataPtr<FBoidStateFragment>(Mate);
			if (!MateGen || !MateState)
			{
				continue;
			}

			// Offspring inherits a mix of both parents' genomes, then mutates.
			const FBoidGenome Child = FBoidGenomeLibrary::Crossover(G, MateGen->Genome, *Config, Rng);
			const FVector BirthPos = (Pos + MatePos) * 0.5f + FVector(Rng.FRandRange(-120.f, 120.f), Rng.FRandRange(-120.f, 120.f), 0.f);
			const int32 ChildGeneration = FMath::Max(S.Generation, MateState->Generation) + 1;
			Sim->RequestBirth(Species, Child, BirthPos, ChildGeneration);

			// Both parents pay the cost and go on cooldown.
			S.CurrentHunger -= Evo::ReproHungerCost * MaxHunger;
			S.ReproCooldown = Evo::ReproCooldown(G);
			MateState->CurrentHunger -= Evo::ReproHungerCost * Evo::MaxHunger(MateGen->Genome);
			MateState->ReproCooldown = Evo::ReproCooldown(MateGen->Genome);
		}
	});
}
