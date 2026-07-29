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
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

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

	// Camera position for the steering LOD, resolved once on the game thread. With no view
	// (headless), bHasCam stays false and every boid recomputes every frame -- LOD disabled.
	FVector CamPos = FVector::ZeroVector;
	bool bHasCam = false;
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		if (PC->PlayerCameraManager)
		{
			CamPos = PC->PlayerCameraManager->GetCameraLocation();
			bHasCam = true;
		}
	}

	EntityQuery.ForEachEntityChunk(Context, [this, Grid, CamPos, bHasCam](FMassExecutionContext& Context)
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
				// Day/night: a creature sees well during its active period and poorly outside it,
				// so a nocturnal hunter owns the night and a diurnal prey is vulnerable in it.
				const float Awake = Evo::Activity(Species.Nocturnality, Evo::GDaylight);
				const float TimeSight = FMath::Lerp(0.55f, 1.15f, Awake);
				const float Radius = Evo::PerceptionRadius(G) * SelfBiome.PerceptionMultiplier * TimeSight;
				const float Need = 1.f - FMath::Clamp(S.CurrentHunger / Evo::MaxHunger(G), 0.f, 1.f);

				// --- LOD CAMERA : la requete de voisinage domine le cout du steering. Un boid
				// proche redecide souvent, un boid lointain (souvent hors-champ) beaucoup plus
				// rarement et reutilise sa derniere force. Le mouvement integre a chaque frame,
				// donc rien ne saute -- seul le RYTHME de decision baisse avec la distance.
				const float CamDistSq = bHasCam ? static_cast<float>(FVector::DistSquaredXY(Pos, CamPos)) : 0.f;
				const uint32 Stride = bHasCam ? static_cast<uint32>(Evo::SteerStride(CamDistSq)) : 1u;
				if (Stride > 1u && ((static_cast<uint32>(Self.Index) + static_cast<uint32>(GFrameCounter)) % Stride) != 0u)
				{
					Force[It].Value = S.CachedSteer;
					continue;
				}

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
				const float SelfMeat = Evo::MeatDigestion(G); // our own place on the carnivory scale

				// Is this individual currently in the market for a partner? Same test the
				// reproduction processor uses, so the state never disagrees with the outcome.
				const bool bWantsMate = (S.Age >= Evo::MaturityAge)
					&& (S.ReproCooldown <= 0.f)
					&& (S.CurrentHunger >= Evo::ReproHungerFraction * Evo::MaxHunger(G));

				FVector MatePos = FVector::ZeroVector;
				float BestMateScore = -1.f;
				bool bHasMate = false;

				// A "predator" is something clearly ABOVE us on the carnivory scale, not merely
				// something that eats meat -- otherwise an omnivore prey would scare its own hunter.
				bool bPredatorNearby = false;

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

							// Sexual selection at the steering level: head for the most attractive
							// available partner in range, so pairs actually meet before breeding.
							if (bWantsMate && Other.bCanMate && Other.Attractiveness > BestMateScore)
							{
								BestMateScore = Other.Attractiveness;
								MatePos = Other.Position;
								bHasMate = true;
							}
						}
						else
						{
							const float Threat = Evo::MeatDigestionFromDiet(Other.Diet);
							if (Threat > SelfMeat + Evo::HuntTierMargin)
							{
								bPredatorNearby = true; // this one could actually eat us
							}
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

				// ---- Behaviour state ------------------------------------------------
				// Single source of truth for CurrentBehaviorState. Strict priority: staying alive
				// beats resting, resting beats breeding, breeding beats eating, eating beats idling.
				{
					const float MaxStam = Evo::MaxStamina(G);
					const bool bThreatened = bPredatorNearby || (S.Adrenaline > 0.f);
					const bool bTooHungryToSleep = S.CurrentHunger < Evo::SleepMinHungerFrac * Evo::MaxHunger(G);
					const bool bWasAsleep = (S.CurrentBehaviorState == EBoidState::Sleeping);

					// Hysteresis: drop off below the enter threshold, stay down until well rested.
					const float SleepThreshold = bWasAsleep ? Evo::SleepWakeStaminaFrac : Evo::SleepEnterStaminaFrac;
					const bool bWantsSleep = !bThreatened && !bTooHungryToSleep
						&& (S.CurrentStamina < SleepThreshold * MaxStam);

					if (bThreatened)
					{
						S.CurrentBehaviorState = EBoidState::Fleeing;
					}
					else if (bWantsSleep)
					{
						S.CurrentBehaviorState = EBoidState::Sleeping;
					}
					else if (bWantsMate && bHasMate)
					{
						S.CurrentBehaviorState = EBoidState::Mating;
					}
					else if (bHasPrey && Need > 0.f)
					{
						S.CurrentBehaviorState = EBoidState::Hunting;
					}
					else if (Need > 0.f)
					{
						S.CurrentBehaviorState = EBoidState::Foraging;
					}
					else
					{
						S.CurrentBehaviorState = EBoidState::Wandering;
					}
				}

				// A sleeping boid produces no steering force at all; the movement processor
				// pins it in place and recovers its stamina.
				if (S.CurrentBehaviorState == EBoidState::Sleeping)
				{
					Force[It].Value = FVector::ZeroVector;
					continue;
				}

				// Cible de debug : repartir de zéro, puis mémoriser celle qu'on poursuit
				// réellement dans les branches ci-dessous (aucune requête supplémentaire).
				S.DebugTargetKind = EBoidDebugTarget::None;

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
						S.DebugTargetPos = Food.Position;
						S.DebugTargetKind = EBoidDebugTarget::Plant;
					}
				}

				if (Evo::CanHunt(G) && Need > 0.f)
				{
					FGridFood Carcass;
					if (Grid->FindNearestFood(Pos, Radius, EFoodType::Carcass, Carcass))
					{
						Steer += (Carcass.Position - Pos).GetSafeNormal() * (Evo::SeekFoodWeight * Need * Evo::MeatDigestion(G));
						S.DebugTargetPos = Carcass.Position;
						S.DebugTargetKind = EBoidDebugTarget::Carcass;
					}
				}

				if (bHasPrey)
				{
					S.DebugTargetPos = NearestPrey;
					S.DebugTargetKind = EBoidDebugTarget::Prey;
					Steer += (NearestPrey - Pos).GetSafeNormal()
						* (Evo::ChaseWeightScale * G.Get(EBoidStat::Aggressiveness) * (0.5f + Need) * Evo::MeatDigestion(G));
				}

				// Courtship: close the distance to the chosen partner so the reproduction
				// processor (which needs them within MatingRadius) has someone to pair with.
				if (S.CurrentBehaviorState == EBoidState::Mating)
				{
					S.DebugTargetPos = MatePos;
					S.DebugTargetKind = EBoidDebugTarget::Mate;
					Steer += (MatePos - Pos).GetSafeNormal() * Evo::SeekPartnerWeight;
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
				S.CachedSteer = Force[It].Value; // reutilisee sur les frames sautees (LOD)
			}
		});
}