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

namespace
{
	/** Overlay colour for each behaviour state, so the swarm reads at a glance. */
	FColor StateColor(EBoidState State)
	{
		switch (State)
		{
		case EBoidState::Foraging:  return FColor(120, 220, 120, 255); // green
		case EBoidState::Hunting:   return FColor(255, 140, 40, 255);  // orange
		case EBoidState::Fleeing:   return FColor(240, 60, 60, 255);   // red
		case EBoidState::Mating:    return FColor(240, 120, 220, 255); // pink
		case EBoidState::Sleeping:  return FColor(90, 140, 255, 255);  // blue
		case EBoidState::Wandering:
		default:                    return FColor(210, 210, 210, 255); // grey
		}
	}
}

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
	EntityQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadOnly);
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

	// 0 = markers, 1 = perception, 2 = behaviour state, 3 = vitals, 4 = everything.
	const int32 Mode = Sim->GetDebugMode();
	const bool bDrawPerception = (Mode == 1 || Mode == 4);
	const bool bDrawState = (Mode == 2 || Mode == 4);
	const bool bDrawVitals = (Mode == 3 || Mode == 4);

	EntityQuery.ForEachEntityChunk(Context, [World, bDrawPerception, bDrawState, bDrawVitals](FMassExecutionContext& Context)
		{
			const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
			const FColor SpeciesColor = Species.Color.ToFColor(true);
			const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FMassVelocityFragment> Vel = Context.GetFragmentView<FMassVelocityFragment>();
			const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
			const TConstArrayView<FBoidStateFragment> St = Context.GetFragmentView<FBoidStateFragment>();

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
				const FBoidGenome& G = Gen[It].Genome;
				const FBoidStateFragment& S = St[It];
				const float Radius = Evo::PerceptionRadius(G);

				// In state mode the marker takes the behaviour colour instead of the species one.
				const FColor MarkerColor = bDrawState ? StateColor(S.CurrentBehaviorState) : SpeciesColor;

				// Bright marker dot at the boid so it's easy to locate.
				DrawDebugPoint(World, Pos, bDrawState ? 13.f : 9.f, MarkerColor, false, -1.f, 0);

				// Bold heading arrow.
				if (V.SizeSquared() > 1.f)
				{
					const FVector End = Pos + V.GetSafeNormal() * FMath::Max(300.f, Radius * 0.3f);
					DrawDebugDirectionalArrow(World, Pos, End, 180.f, MarkerColor, false, -1.f, 0, Evo::DebugArrowThickness);
				}

				if (bDrawPerception)
				{
					// Crisp, high-contrast perception circle (white core + species-tinted rim for depth).
					DrawDebugCircle(World, Pos, Radius, 32, FColor(245, 250, 255, 255), false, -1.f, 0, Evo::DebugCircleThickness,
						FVector(1, 0, 0), FVector(0, 1, 0), false);
					DrawDebugCircle(World, Pos, Radius * 0.985f, 32, SpeciesColor, false, -1.f, 0, Evo::DebugCircleThickness * 0.6f,
						FVector(1, 0, 0), FVector(0, 1, 0), false);
				}

				if (bDrawState)
				{
					// A ring in the state colour, so clusters of behaviour stand out from above.
					DrawDebugCircle(World, Pos, 150.f, 20, MarkerColor, false, -1.f, 0, Evo::DebugCircleThickness,
						FVector(1, 0, 0), FVector(0, 1, 0), false);
				}

				if (bDrawVitals)
				{
					// Two stacked bars: HP on top (red -> green), hunger below (amber).
					constexpr float BarHalfWidth = 130.f;
					const FVector Left = Pos + FVector(0.f, -BarHalfWidth, 120.f);
					const FVector Right = Pos + FVector(0.f, BarHalfWidth, 120.f);

					const float HPFrac = FMath::Clamp(S.CurrentHP / Evo::MaxHP(G), 0.f, 1.f);
					const float HungerFrac = FMath::Clamp(S.CurrentHunger / Evo::MaxHunger(G), 0.f, 1.f);

					DrawDebugLine(World, Left, Right, FColor(30, 30, 30, 160), false, -1.f, 0, 5.f);
					DrawDebugLine(World, Left, FMath::Lerp(Left, Right, HPFrac),
						FColor(FMath::RoundToInt(255 * (1.f - HPFrac)), FMath::RoundToInt(255 * HPFrac), 60, 255),
						false, -1.f, 0, 5.f);

					const FVector Left2 = Left - FVector(0.f, 0.f, 45.f);
					const FVector Right2 = Right - FVector(0.f, 0.f, 45.f);
					DrawDebugLine(World, Left2, Right2, FColor(30, 30, 30, 160), false, -1.f, 0, 5.f);
					DrawDebugLine(World, Left2, FMath::Lerp(Left2, Right2, HungerFrac), FColor(240, 180, 40, 255),
						false, -1.f, 0, 5.f);
				}
			}
		});
}