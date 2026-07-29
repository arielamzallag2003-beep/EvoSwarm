// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "BoidFragments.h"
#include "EvoswarmTuning.h"
#include "BoidGridSubsystem.h"

UFoodDecayProcessor::UFoodDecayProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidFeedingProcessor"));
}

void UFoodDecayProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FFoodTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FFoodFragment>(EMassFragmentAccess::ReadWrite);
}

void UFoodDecayProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UBoidGridSubsystem* Grid = World ? World->GetSubsystem<UBoidGridSubsystem>() : nullptr;

	EntityQuery.ForEachEntityChunk(Context, [Grid](FMassExecutionContext& Context)
	{
		const float Dt = Context.GetDeltaTimeSeconds();
		const TArrayView<FFoodFragment> Food = Context.GetMutableFragmentView<FFoodFragment>();
		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			// Plants persist until grazed; carcasses rot.
			if (Food[It].Type != EFoodType::Carcass)
			{
				continue;
			}
			Food[It].Energy -= Evo::CarcassDecayPerSec * Dt;
			if (Food[It].Energy <= 0.f)
			{
				// Claim guards against double-destroy if a feeder finished this carcass this frame.
				const FMassEntityHandle Self = Context.GetEntity(It);
				if (!Grid || Grid->TryClaim(Self))
				{
					Context.Defer().DestroyEntity(Self);
				}
			}
		}
	});
}
