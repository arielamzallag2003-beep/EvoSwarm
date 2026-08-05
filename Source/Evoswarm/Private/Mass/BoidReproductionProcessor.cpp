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

			// Doit être mature, reposé, nourri et disponible pour pouvoir se reproduire.
			if (S.Age < Evo::MaturityAge || S.CurrentFatigue >= 0.8f || S.ReproCooldown > 0.f || S.CurrentHunger < Evo::ReproHungerFraction * MaxHunger)
			{
				S.TargetPartner.Reset();
				continue;
			}

			const FMassEntityHandle Self = Context.GetEntity(It);
			const FVector Pos = Xf[It].GetTransform().GetLocation();

			// Sexual selection: among ready same-species partners in range, choose the one with
			// the higher attractiveness (see ComputeAttractivenessScore), and lower genetic distance
			FMassEntityHandle BestMate;
			float BestScore = -1.f;
			bool bFoundMate = false;
			
			if (S.TargetPartner.IsValid() && EntityManager.IsEntityValid(S.TargetPartner))
			{
				BestMate = S.TargetPartner;
				BestScore = S.TargetPartnerAttractiveness;
				bFoundMate = true;
			}
			
			Grid->QueryAgents(Pos, Evo::MatingRadius, [&](const FGridAgent& Other)
			{
				// On ignore soi-même, le partenaire actuel s'il existe, les membres d'une autre espèces et les individus qui ne peuvent pas se reproduire
				if (Other.SpeciesIndex != Species.SpeciesIndex || !Other.bCanMate || Other.Entity == Self || Other.Entity == BestMate)
				{
					return;
				}

				const FBoidGenomeFragment* OtherGenFragment = EntityManager.GetFragmentDataPtr<FBoidGenomeFragment>(Other.Entity);
				if (!OtherGenFragment)
				{
					return; // Sécurité si l'entité est en train de disparaître
				}
    
				const FBoidGenome& OtherGenome = OtherGenFragment->Genome;

				// Score d'attractivité
				float AttractionScore = Other.Attractiveness;
				float TotalNormalizedDiff = 0.f;

				// Différence génétique normalisée par statistique
				for (int32 Index = 0; Index < NumBoidStats; ++Index)
				{
					const FBoidStatDef Def = Config->GetStatDef(static_cast<EBoidStat>(Index));
					const float Range = FMath::Max(Def.Min, Def.Max) - Def.Min;

					if (Range > KINDA_SMALL_NUMBER)
					{
						const float RawDiff = FMath::Abs(G.Stats[Index] - OtherGenome.Stats[Index]);
						TotalNormalizedDiff += (RawDiff / Range);
					}
				}

				float GenomeDifferenceFraction = TotalNormalizedDiff / static_cast<float>(NumBoidStats);
				AttractionScore -= (GenomeDifferenceFraction * 0.5f);

				if (AttractionScore > BestScore)
				{
					BestScore = AttractionScore;
					BestMate = Other.Entity;
					bFoundMate = true;
				}
			});

			if (bFoundMate)
			{
				S.TargetPartner = BestMate;
				S.TargetPartnerAttractiveness = BestScore;
			}
			else
			{
				S.TargetPartner.Reset();
				S.TargetPartnerAttractiveness = 0.f;
				continue; // no available partner this frame
			}
			
			// --- TENTATIVE D'ACCOUPLEMENT PHYSIQUE ---

			if (!EntityManager.IsEntityValid(BestMate))
			{
				S.TargetPartner.Reset();
				S.TargetPartnerAttractiveness = 0.f;
				continue;
			}

			const FTransformFragment* MateXf = EntityManager.GetFragmentDataPtr<FTransformFragment>(BestMate);
			FBoidGenomeFragment* MateGen = EntityManager.GetFragmentDataPtr<FBoidGenomeFragment>(BestMate);
			FBoidStateFragment* MateState = EntityManager.GetFragmentDataPtr<FBoidStateFragment>(BestMate);
			
			if (!MateXf || !MateState || !MateGen)
			{
				S.TargetPartner.Reset();
				S.TargetPartnerAttractiveness = 0.f;
				continue;
			}

			// Calcul de la distance d'accouplement dynamique (fraction de la perception minimale entre les deux)
			const float MyRadius = Evo::PerceptionRadius(G);
			const float MateRadius = Evo::PerceptionRadius(MateGen->Genome);
			const float MinPerceptionRadius = FMath::Min(MyRadius, MateRadius);
			
			// Seuil d'accouplement : 15% du rayon de perception minimal commun
			const float MatingFraction = 0.15f; 
			const float SafeMatingDistanceSq = FMath::Square(MinPerceptionRadius * MatingFraction);

			const FVector MatePos = MateXf->GetTransform().GetLocation();
			const float DistanceSq = FVector::DistSquared(Pos, MatePos);

			if (DistanceSq > SafeMatingDistanceSq)
			{
				// Trop loin : On laisse le Steering rapprocher les boids.
				continue; 
			}
			
			// Les deux boids doivent être disponibles pour s'unir à cet instant t
			if (!Grid->TryClaim(Self) || !Grid->TryClaim(BestMate))
			{
				continue; // L'un d'eux est déjà occupé par un accouplement validé cette frame
			}
			
			// === CONCEPTION --> CROSSOVER + MUTATION ===
			// 1. Le mélange (40/40/19/1)
			FBoidGenome Child = FBoidGenomeLibrary::Crossover(G, MateGen->Genome, *Config, Rng);
			// 2. La micro-mutation sur l'enfant
			Child = FBoidGenomeLibrary::Mutate(Child, *Config, Rng);

			const FVector BirthPos = (Pos + MatePos) * 0.5f + FVector(Rng.FRandRange(-120.f, 120.f), Rng.FRandRange(-120.f, 120.f), 0.f);
			const int32 ChildGeneration = FMath::Max(S.Generation, MateState->Generation) + 1;
			Sim->RequestBirth(Species, Child, BirthPos, ChildGeneration);

			// Consommation et historique
			S.ReproductionCount++;
			MateState->ReproductionCount++;
			
			// Both parents pay the cost and go on cooldown.
			S.CurrentHunger -= Evo::ReproHungerCost * MaxHunger;
			S.ReproCooldown = Evo::ReproCooldown(G);
			MateState->CurrentHunger -= Evo::ReproHungerCost * Evo::MaxHunger(MateGen->Genome);
			MateState->ReproCooldown = Evo::ReproCooldown(MateGen->Genome);

			// Réinitialisation après succès
			S.TargetPartner.Reset();
			S.TargetPartnerAttractiveness = 0.f;
			MateState->TargetPartner.Reset();
			MateState->TargetPartnerAttractiveness = 0.f;
		}
	});
}
