// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"
#include "EvoswarmTerrain.h"
#include "BoidGridSubsystem.h"

UBoidGridUpdateProcessor::UBoidGridUpdateProcessor()
	: BoidQuery(*this)
	, FoodQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true; // writes to the shared grid subsystem
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UBoidGridUpdateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	BoidQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	BoidQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	BoidQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	BoidQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);
	BoidQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadOnly);
	BoidQuery.AddSharedRequirement<FBoidSpeciesSharedFragment>(EMassFragmentAccess::ReadOnly);

	FoodQuery.AddTagRequirement<FFoodTag>(EMassFragmentPresence::All);
	FoodQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	FoodQuery.AddRequirement<FFoodFragment>(EMassFragmentAccess::ReadOnly);
}

void UBoidGridUpdateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UBoidGridSubsystem* Grid = World ? World->GetSubsystem<UBoidGridSubsystem>() : nullptr;
	if (!Grid)
	{
		return;
	}

	Grid->BeginFrame();

	BoidQuery.ForEachEntityChunk(Context, [Grid](FMassExecutionContext& Context)
	{
		const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
		const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassVelocityFragment> Vel = Context.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
		const TConstArrayView<FBoidStateFragment> States = Context.GetFragmentView<FBoidStateFragment>();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			const FBoidGenome& G = Gen[It].Genome;
			const FBoidStateFragment& S = States[It];
			FGridAgent Agent;
			Agent.Position = Xf[It].GetTransform().GetLocation();
			Agent.Velocity = Vel[It].Value;
			Agent.Entity = Context.GetEntity(It);
			Agent.SpeciesIndex = Species.SpeciesIndex;
			Agent.Biomass = G.Get(EBoidStat::Biomass);
			// Effective stealth includes biome cover (forests conceal).
			Agent.Stealth = G.Get(EBoidStat::Stealth) + Evo::GetBiomeParams(Evo::BiomeAt(Agent.Position.X, Agent.Position.Y)).StealthBonus;
			Agent.Diet = G.Get(EBoidStat::Diet);
			Agent.Intimidation = G.Get(EBoidStat::Intimidation);
			Agent.bCanMate = (S.Age >= Evo::MaturityAge) && (S.ReproCooldown <= 0.f)
				&& (S.CurrentHunger >= Evo::ReproHungerFraction * Evo::MaxHunger(G)) && (Evo::Fatigue(G, S.CurrentStamina) < Evo::MateMaxFatigue);
			// Attractiveness is based on health, age and reproduction count
			// so mate choice toward high attractiveness favours successful genomes.
			Agent.Attractiveness = FBoidGenomeLibrary::ComputeAttractivenessScore(G, S);
			Grid->AddAgent(Agent);
		}
	});

	FoodQuery.ForEachEntityChunk(Context, [Grid](FMassExecutionContext& Context)
	{
		const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FFoodFragment> Food = Context.GetFragmentView<FFoodFragment>();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			FGridFood Node;
			Node.Position = Xf[It].GetTransform().GetLocation();
			Node.Entity = Context.GetEntity(It);
			Node.Energy = Food[It].Energy;
			Node.Type = Food[It].Type;
			Grid->AddFood(Node);
		}
	});
}
