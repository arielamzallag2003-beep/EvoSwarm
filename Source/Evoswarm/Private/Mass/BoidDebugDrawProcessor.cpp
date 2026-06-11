// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"
#include "EvoswarmTerrain.h"
#include "EvoswarmSimSubsystem.h"
#include "DrawDebugHelpers.h"

UBoidDebugDrawProcessor::UBoidDebugDrawProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true; // immediate-mode debug drawing
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidMovementProcessor"));
}

void UBoidDebugDrawProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSharedRequirement<FBoidSpeciesSharedFragment>(EMassFragmentAccess::ReadOnly);
}

void UBoidDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	if (!World || !Sim || !Sim->IsDebugDraw())
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context, [World](FMassExecutionContext& Context)
	{
		const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
		const FColor SpeciesColor = Species.Color.ToFColor(true);
		const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassVelocityFragment> Vel = Context.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			// Only a sparse sample, so the view stays readable.
			if (Context.GetEntity(It).Index % Evo::DebugSampleStride != 0)
			{
				continue;
			}

			// Lift overlays above the body so the terrain/creatures don't hide them.
			const FVector Pos = Xf[It].GetTransform().GetLocation() + FVector(0, 0, Evo::DebugZLift);
			const FVector V = Vel[It].Value;
			const float Radius = Evo::PerceptionRadius(Gen[It].Genome);

			// Bright marker dot at the boid so it's easy to locate.
			DrawDebugPoint(World, Pos, 9.f, SpeciesColor, false, -1.f, 0);

			// Bold heading arrow (species-coloured).
			if (V.SizeSquared() > 1.f)
			{
				const FVector End = Pos + V.GetSafeNormal() * FMath::Max(300.f, Radius * 0.3f);
				DrawDebugDirectionalArrow(World, Pos, End, 180.f, SpeciesColor, false, -1.f, 0, Evo::DebugArrowThickness);
			}

			// Crisp, high-contrast perception circle (white core + species-tinted rim for depth).
			DrawDebugCircle(World, Pos, Radius, 32, FColor(245, 250, 255, 255), false, -1.f, 0, Evo::DebugCircleThickness,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
			DrawDebugCircle(World, Pos, Radius * 0.985f, 32, SpeciesColor, false, -1.f, 0, Evo::DebugCircleThickness * 0.6f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
		}
	});
}
