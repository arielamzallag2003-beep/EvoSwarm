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

UBoidSteeringProcessor::UBoidSteeringProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidGridUpdateProcessor"));
}

void UBoidSteeringProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadWrite); // drifts WanderAngle
	EntityQuery.AddSharedRequirement<FBoidSpeciesSharedFragment>(EMassFragmentAccess::ReadOnly);
}

void UBoidSteeringProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UBoidGridSubsystem* Grid = World ? World->GetSubsystem<UBoidGridSubsystem>() : nullptr;
	if (!Grid)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context, [this, Grid](FMassExecutionContext& Context)
	{
		const float Dt = Context.GetDeltaTimeSeconds();
		const float CosHalfFOV = FMath::Cos(FMath::DegreesToRadians(Evo::PerceptionFOVDegrees * 0.5f));
		const float WanderDrift = FMath::DegreesToRadians(Evo::WanderDriftDegPerSec) * Dt;

		const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
		const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassVelocityFragment> Vel = Context.GetFragmentView<FMassVelocityFragment>();
		const TArrayView<FMassForceFragment> Force = Context.GetMutableFragmentView<FMassForceFragment>();
		const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
		const TArrayView<FBoidStateFragment> States = Context.GetMutableFragmentView<FBoidStateFragment>();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			const FBoidGenome& G = Gen[It].Genome;
			FBoidStateFragment& S = States[It];
			const FVector Pos = Xf[It].GetTransform().GetLocation();
			const FVector MyVel = Vel[It].Value;
			const FMassEntityHandle Self = Context.GetEntity(It);

			const FBiomeParams SelfBiome = Evo::GetBiomeParams(Evo::BiomeAt(Pos.X, Pos.Y));
			const float Radius = Evo::PerceptionRadius(G) * SelfBiome.PerceptionMultiplier;
			const float Need = 1.f - FMath::Clamp(S.CurrentHunger / Evo::MaxHunger(G), 0.f, 1.f);

			// Field of view: only perceive what's roughly ahead. A still boid sees all around.
			const bool bMoving = MyVel.SizeSquared() > 1.f;
			const FVector Forward = bMoving ? MyVel.GetSafeNormal() : FVector::ZeroVector;

			FVector Separation = FVector::ZeroVector;
			FVector AlignSum = FVector::ZeroVector;
			FVector CohesionSum = FVector::ZeroVector;
			FVector FleeSum = FVector::ZeroVector;
			float FlockWeight = 0.f;
			FVector NearestPrey = FVector::ZeroVector;
			float BestPreyDistSq = TNumericLimits<float>::Max();
			bool bHasPrey = false;

			const bool bCanHunt = Evo::CanHunt(G);

			Grid->QueryAgents(Pos, Radius, [&](const FGridAgent& Other)
			{
				if (Other.Entity == Self)
				{
					return;
				}
				const FVector Delta = Other.Position - Pos;
				const float Dist = Delta.Size() + KINDA_SMALL_NUMBER;
				const FVector Dir = Delta / Dist;

				// Outside the forward field of view -> not perceived (reduces confusion).
				if (bMoving && FVector::DotProduct(Forward, Dir) < CosHalfFOV)
				{
					return;
				}

				// Closer neighbours matter more (smooth distance falloff).
				const float Weight = FMath::Clamp(1.f - Dist / Radius, 0.f, 1.f);

				if (Other.SpeciesIndex == Species.SpeciesIndex)
				{
					if (Dist < Evo::SeparationRadius)
					{
						Separation -= Dir * ((Evo::SeparationRadius - Dist) / Evo::SeparationRadius);
					}
					AlignSum += Other.Velocity * Weight;
					CohesionSum += Other.Position * Weight;
					FlockWeight += Weight;
				}
				else
				{
					const float Threat = Evo::MeatDigestionFromDiet(Other.Diet);
					if (Threat > Evo::DietEfficiencyFloor)
					{
						const float Awareness = FMath::Clamp(1.f - Other.Stealth * 0.5f, 0.f, 1.f);
						const float Fear = 1.f + Other.Intimidation * Evo::IntimidationFleeScale;
						FleeSum -= Dir * (Awareness * Fear * Threat * Weight);
					}
					if (bCanHunt)
					{
						const float DistSq = Dist * Dist;
						if (DistSq < BestPreyDistSq)
						{
							BestPreyDistSq = DistSq;
							NearestPrey = Other.Position;
							bHasPrey = true;
						}
					}
				}
			});

			FVector Steer = Separation * Evo::SeparationWeight;

			if (FlockWeight > KINDA_SMALL_NUMBER)
			{
				const FVector AvgVel = AlignSum / FlockWeight;
				const FVector AvgPos = CohesionSum / FlockWeight;
				Steer += (AvgVel - MyVel).GetSafeNormal() * Evo::AlignmentWeight;
				Steer += (AvgPos - Pos).GetSafeNormal() * (Evo::CohesionWeightScale * G.Get(EBoidStat::Integration));
			}

			Steer += FleeSum * Evo::FleeWeight;

			if (Evo::CanEatPlants(G) && Need > 0.f)
			{
				FGridFood Food;
				if (Grid->FindNearestFood(Pos, Radius, EFoodType::Plant, Food))
				{
					Steer += (Food.Position - Pos).GetSafeNormal() * (Evo::SeekFoodWeight * Need * Evo::PlantDigestion(G));
				}
			}

			if (Evo::CanHunt(G) && Need > 0.f)
			{
				FGridFood Carcass;
				if (Grid->FindNearestFood(Pos, Radius, EFoodType::Carcass, Carcass))
				{
					Steer += (Carcass.Position - Pos).GetSafeNormal() * (Evo::SeekFoodWeight * Need * Evo::MeatDigestion(G));
				}
			}

			if (bHasPrey)
			{
				Steer += (NearestPrey - Pos).GetSafeNormal()
					* (Evo::ChaseWeightScale * G.Get(EBoidStat::Aggressiveness) * (0.5f + Need) * Evo::MeatDigestion(G));
			}

			// Smooth meandering wander: a heading that drifts slowly instead of per-frame jitter.
			S.WanderAngle += Rng.FRandRange(-1.f, 1.f) * WanderDrift;
			Steer += FVector(FMath::Cos(S.WanderAngle), FMath::Sin(S.WanderAngle), 0.f) * (Evo::WanderAccel / Evo::MaxSteerAccel);

			// Water is swimmable, not a wall — but also not a place to live (no food there).
			if (Evo::TerrainHeight(Pos.X, Pos.Y) < Evo::SeaLevel + Evo::WaterEdgeMargin)
			{
				// Already in water: ALWAYS head firmly to the nearest dry land, so nobody gets
				// trapped and starves in open water. A strong food/prey pull can still carry a good
				// swimmer across, but with no goal they make for shore.
				const FVector ToLand = Evo::ToHigherGround(Pos.X, Pos.Y, Evo::WaterEscapeProbe);
				if (!ToLand.IsNearlyZero())
				{
					Steer += ToLand * (Evo::WaterEscapeAccel / Evo::MaxSteerAccel);
				}
			}
			else
			{
				// On land heading toward water: reluctance to enter scales with adaptation, so a
				// strong swimmer wades in to forage/chase while a poor swimmer keeps to dry ground.
				const FVector Ahead = Pos + (bMoving ? Forward : FVector(1.f, 0.f, 0.f)) * Evo::WaterLookAhead;
				if (Evo::TerrainHeight(Ahead.X, Ahead.Y) < Evo::SeaLevel + Evo::WaterEdgeMargin)
				{
					const float Reluctance = 1.f - Evo::AquaticAdaptation(G);
					if (Reluctance > 0.02f)
					{
						const FVector ToLand = Evo::ToHigherGround(Pos.X, Pos.Y, Evo::WaterLookAhead);
						if (!ToLand.IsNearlyZero())
						{
							Steer += ToLand * (Reluctance * Evo::WaterAvoidAccel / Evo::MaxSteerAccel);
						}
					}
				}
			}

			// Turn back near the arena edge.
			const float EdgeX = Evo::ArenaHalfExtent - FMath::Abs(Pos.X);
			const float EdgeY = Evo::ArenaHalfExtent - FMath::Abs(Pos.Y);
			if (EdgeX < Evo::BoundsMargin)
			{
				Steer.X += -FMath::Sign(Pos.X) * (Evo::BoundsTurnAccel / Evo::MaxSteerAccel) * (1.f - EdgeX / Evo::BoundsMargin);
			}
			if (EdgeY < Evo::BoundsMargin)
			{
				Steer.Y += -FMath::Sign(Pos.Y) * (Evo::BoundsTurnAccel / Evo::MaxSteerAccel) * (1.f - EdgeY / Evo::BoundsMargin);
			}

			Steer.Z = 0.f;
			Force[It].Value = Steer.GetClampedToMaxSize(1.f) * Evo::MaxSteerAccel;
		}
	});
}
