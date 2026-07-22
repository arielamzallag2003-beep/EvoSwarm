// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"
#include "BoidGridSubsystem.h"
#include "EvoswarmSimSubsystem.h"

UBoidFeedingProcessor::UBoidFeedingProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidGridUpdateProcessor"));
}

void UBoidFeedingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSharedRequirement<FBoidSpeciesSharedFragment>(EMassFragmentAccess::ReadOnly);
}

void UBoidFeedingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UBoidGridSubsystem* Grid = World ? World->GetSubsystem<UBoidGridSubsystem>() : nullptr;
	UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	if (!Grid)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager, Grid, Sim](FMassExecutionContext& Context)
		{
			const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
			const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
			const TArrayView<FBoidStateFragment> State = Context.GetMutableFragmentView<FBoidStateFragment>();

			for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
			{
				const FBoidGenome& G = Gen[It].Genome;
				const FVector Pos = Xf[It].GetTransform().GetLocation();
				FBoidStateFragment& S = State[It];
				const float MaxHunger = Evo::MaxHunger(G);

				// A sleeping boid neither grazes, scavenges nor hunts.
				if (S.CurrentBehaviorState == EBoidState::Sleeping)
				{
					continue;
				}

				// Plant-eaters graze the nearest plant (consumed whole).
				if (Evo::CanEatPlants(G) && S.CurrentHunger < MaxHunger)
				{
					FGridFood Food;
					if (Grid->FindNearestFood(Pos, Evo::EatReach, EFoodType::Plant, Food) && Grid->TryClaim(Food.Entity))
					{
						S.CurrentHunger = FMath::Min(MaxHunger, S.CurrentHunger + Food.Energy * Evo::PlantDigestion(G));
						Context.Defer().DestroyEntity(Food.Entity);
						if (Sim)
						{
							Sim->NotifyFoodConsumed();
						}
					}
				}

				// Meat-eaters scavenge carcasses: take a bite, sharing it over time (packs feed together).
				if (Evo::CanHunt(G) && S.CurrentHunger < MaxHunger)
				{
					FGridFood Carcass;
					if (Grid->FindNearestFood(Pos, Evo::EatReach, EFoodType::Carcass, Carcass) && Grid->TryClaim(Carcass.Entity))
					{
						if (FFoodFragment* CarcFrag = EntityManager.GetFragmentDataPtr<FFoodFragment>(Carcass.Entity))
						{
							const float Bite = FMath::Min(Evo::CarcassBite, CarcFrag->Energy);
							S.CurrentHunger = FMath::Min(MaxHunger, S.CurrentHunger + Bite * Evo::MeatDigestion(G));
							CarcFrag->Energy -= Bite;
							if (CarcFrag->Energy <= 0.f)
							{
								Context.Defer().DestroyEntity(Carcass.Entity);
							}
						}
					}
				}

				// A boid running for its life or courting a partner does not start a fight.
				const bool bInRestrictiveState =
					(S.CurrentBehaviorState == EBoidState::Fleeing) ||
					(S.CurrentBehaviorState == EBoidState::Mating);

				// Live hunting: stamina-gated, pack-boosted; prey fights back; a kill drops a carcass.
				if (Evo::CanHunt(G) && !bInRestrictiveState
					&& S.AttackCooldown <= 0.f && S.CurrentStamina >= Evo::MinStaminaToAttack)
				{
					FMassEntityHandle PreyHandle;
					FVector PreyPos = FVector::ZeroVector;
					int32 PreySpecies = INDEX_NONE;
					float BestDistSq = TNumericLimits<float>::Max();
					bool bFound = false;
					const float SelfMeat = Evo::MeatDigestion(G); // our place on the carnivory scale

					Grid->QueryAgents(Pos, Evo::EatReach, [&](const FGridAgent& Other)
						{
							if (Other.SpeciesIndex == Species.SpeciesIndex)
							{
								return;
							}
							// Food chain: only hunt prey clearly lower on the carnivory scale (so apex can't farm its own tier).
							if (Evo::MeatDigestionFromDiet(Other.Diet) > SelfMeat - Evo::HuntTierMargin)
							{
								return;
							}
							const float DistSq = FVector::DistSquared(Other.Position, Pos);
							if (DistSq < BestDistSq)
							{
								BestDistSq = DistSq;
								PreyHandle = Other.Entity;
								PreyPos = Other.Position;
								PreySpecies = Other.SpeciesIndex;
								bFound = true;
							}
						});

					if (bFound && Grid->TryClaim(PreyHandle))
					{
						FBoidGenomeFragment* PreyGen = EntityManager.GetFragmentDataPtr<FBoidGenomeFragment>(PreyHandle);
						FBoidStateFragment* PreySt = EntityManager.GetFragmentDataPtr<FBoidStateFragment>(PreyHandle);
						if (PreyGen && PreySt)
						{
							// Pack bonus: more same-species allies nearby + higher Integration = harder hit.
							int32 Allies = 0;
							Grid->QueryAgents(Pos, Evo::PackRadius, [&](const FGridAgent& A)
								{
									if (A.SpeciesIndex == Species.SpeciesIndex)
									{
										++Allies;
									}
								});
							const float PackInstinct = FMath::Clamp(G.Get(EBoidStat::Integration) * 0.08f, 0.f, 1.f);
							const float PackMult = 1.f + FMath::Min(FMath::Max(0, Allies - 1), Evo::PackMaxAllies) * Evo::PackDamagePerAlly * PackInstinct;

							// Strike.
							const float Attack = G.Get(EBoidStat::Damage) * Evo::DamageScale * PackMult;
							const float Defense = PreyGen->Genome.Get(EBoidStat::Armor) * Evo::ArmorScale;
							PreySt->CurrentHP -= FMath::Max(1.f, Attack - Defense);
							PreySt->Adrenaline = Evo::AdrenalineDuration; // prey bolts

							// Being attacked wakes the prey up.
							if (PreySt->CurrentBehaviorState == EBoidState::Sleeping)
							{
								PreySt->CurrentBehaviorState = EBoidState::Fleeing;
							}

							// Prey fights back, scaled by its Aggressiveness.
							const float PreyAggr = FMath::Clamp(PreyGen->Genome.Get(EBoidStat::Aggressiveness) * 0.08f, 0.f, 1.f);
							const float Counter = PreyGen->Genome.Get(EBoidStat::Damage) * Evo::DamageScale * Evo::CounterDamageScale * PreyAggr;
							S.CurrentHP -= FMath::Max(0.f, Counter - G.Get(EBoidStat::Armor) * Evo::ArmorScale);

							// Attacking costs stamina and triggers a cooldown.
							S.CurrentStamina = FMath::Max(0.f, S.CurrentStamina - Evo::AttackStaminaCost);
							S.CurrentHunger = FMath::Max(0.f, S.CurrentHunger - Evo::AttackHungerCost);
							S.AttackCooldown = Evo::AttackCooldownTime;

							if (PreySt->CurrentHP <= 0.f)
							{
								// Drop a carcass at the kill site; the hunter (and pack/scavengers) feed from it.
								if (Sim)
								{
									Sim->RequestCarcass(PreyPos, PreyGen->Genome.Get(EBoidStat::Biomass) * Evo::CarcassEnergyScale);
									Sim->NotifyKill(Species.SpeciesIndex, PreySpecies); // kill for us, predation death for the prey
								}
								Context.Defer().DestroyEntity(PreyHandle);
							}
						}
					}
				}
			}
		});
}