// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"
#include "EvoswarmTerrain.h"
#include "MassCommonFragments.h"
#include "BoidGridSubsystem.h"

UBoidMetabolismProcessor::UBoidMetabolismProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidFeedingProcessor"));
}

void UBoidMetabolismProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadWrite);
}

void UBoidMetabolismProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UBoidGridSubsystem* Grid = World ? World->GetSubsystem<UBoidGridSubsystem>() : nullptr;

	EntityQuery.ForEachEntityChunk(Context, [Grid](FMassExecutionContext& Context)
	{
		const float Dt = Context.GetDeltaTimeSeconds();
		const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
		const TArrayView<FBoidStateFragment> State = Context.GetMutableFragmentView<FBoidStateFragment>();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			const FBoidGenome& G = Gen[It].Genome;
			FBoidStateFragment& S = State[It];
			const FVector Pos = Xf[It].GetTransform().GetLocation();

			S.Age += Dt;
			S.ReproCooldown = FMath::Max(0.f, S.ReproCooldown - Dt);
			S.AttackCooldown = FMath::Max(0.f, S.AttackCooldown - Dt);
			S.Adrenaline = FMath::Max(0.f, S.Adrenaline - Dt);

			// Harsher biomes (desert, highlands) burn food faster.
			const FBiomeParams Biome = Evo::GetBiomeParams(Evo::BiomeAt(Pos.X, Pos.Y));
			const float CarnivoreBurn = FMath::Lerp(1.f, Evo::CarnivoreHungerMult, Evo::MeatDigestion(G));
			S.CurrentHunger -= Evo::HungerDrainPerSec * Biome.HungerDrainMultiplier * CarnivoreBurn * Dt;
			if (S.CurrentHunger <= 0.f)
			{
				S.CurrentHunger = 0.f;
				S.CurrentHP -= Evo::StarvationDamagePerSec * Dt; // starving
			}
			else
			{
				S.CurrentHP = FMath::Min(Evo::MaxHP(G),
					S.CurrentHP + G.Get(EBoidStat::Regeneration) * Evo::RegenPerSecScale * Dt);
			}

			if (S.CurrentHP <= 0.f || S.Age > Evo::Lifespan(G))
			{
				// Claim guards against double-destroy if this boid was also eaten this frame.
				const FMassEntityHandle Self = Context.GetEntity(It);
				if (!Grid || Grid->TryClaim(Self))
				{
					Context.Defer().DestroyEntity(Self);
				}
			}
		}
	});
}
