// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "BoidFragments.h"
#include "BoidGridSubsystem.h"
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
	// --- AJOUT : Accès aux états pour le débug des comportements ---
	EntityQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSharedRequirement<FBoidSpeciesSharedFragment>(EMassFragmentAccess::ReadOnly);
}

void UBoidDebugDrawProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	// --- AJOUT : Récupération du Grid Subsystem pour localiser la nourriture ---
	UBoidGridSubsystem* Grid = World ? World->GetSubsystem<UBoidGridSubsystem>() : nullptr;

	// Suppression du garde-fou global pour permettre l'affichage permanent
	if (!World || !Sim || !Grid)
	{
		return;
	}
	
	// Récupération du mode de debug courant (0 = Off/Standard, 1 = couleur de l'état seule sans vitesse ni fatigue
	// 2 = couleur de l'état et flèche cibles, 3 = couleur espèce et flèches cibles, 4 = Perceptions/Espèces)
	// Si ton subsystem renvoie un int via GetDebugMode(), sinon bIsGlobalDebugActive
	const int32 DebugMode = Sim->GetDebugMode();

	EntityQuery.ForEachEntityChunk(Context, [World, &EntityManager, Grid, DebugMode](FMassExecutionContext& Context)
	{
		const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
		const FColor SpeciesColor = Species.Color.ToFColor(true);
		const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassVelocityFragment> Vel = Context.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
		// --- AJOUT : Lecture de la vue des états ---
		const TConstArrayView<FBoidStateFragment> States = Context.GetFragmentView<FBoidStateFragment>();
		
		// Helper lambda pour raccourcir un vecteur cible afin que la pointe de la flèche soit visible
		auto GetShortenedTarget = [](const FVector& Start, const FVector& End, float ShortenRatio = 0.95f) -> FVector
		{
			FVector Dir = End - Start;
			return Start + Dir * ShortenRatio;
		};
		
		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			const FBoidStateFragment& S = States[It];
			// Lift overlays above the body so the terrain/creatures don't hide them.
			const FVector Pos = Xf[It].GetTransform().GetLocation() + FVector(0, 0, Evo::DebugZLift);
			const FVector V = Vel[It].Value;
			const float Radius = Evo::PerceptionRadius(Gen[It].Genome);

			if (DebugMode == 0)
			{
				continue;
			}
			
			// =========================================================================
			// --- SELECTION DE LA COULEUR SELON L'ÉTAT (TES PRÉFÉRENCES) ---
			// =========================================================================
			FColor BehaviorColor = FColor::White;
			switch (S.CurrentBehaviorState)
			{
				case EBoidState::Wandering: BehaviorColor = FColor::Green; break;
				case EBoidState::Foraging:  BehaviorColor = FColor::Yellow; break;
				case EBoidState::Fleeing:   BehaviorColor = FColor::Red; break;
				case EBoidState::Mating:    BehaviorColor = FColor::Magenta; break; // Rose dans UE
				case EBoidState::Sleeping:  BehaviorColor = FColor::Black; break;
			}
			
			if (DebugMode == 1 || DebugMode == 2)
			{
				// Point lumineux sur le boid avec sa couleur d'état actuelle
				DrawDebugPoint(World, Pos, 25.f, BehaviorColor, false, -1.f, 0);
				// Remplacement de DrawDebugPoint par DrawDebugSphere pour avoir de vraies sphères 3D
				// Rayon de 35.f pour qu'elles soient bien volumineuses, avec 8 segments pour la performance
				//DrawDebugSphere(World, Pos, 35.f, 8, BehaviorColor, false, -1.f, 0, 1.0f);
			}

			if (DebugMode == 2)
			{
				// =========================================================================
				// --- AJOUT : AFFICHAGE DU TEXTE DE DEBUG (Vitesse et Fatigue) ---
				// =========================================================================
				// Calcul des valeurs réelles
				const float CurrentSpeed = V.Size();
				const float CurrentFatigue = S.CurrentFatigue;

				// Formatage du texte : "V: [Vitesse] | F: [Fatigue]"
				// Ex: "V: 245.3 | F: 0.42"
				FString DebugText = FString::Printf(TEXT("V: %.1f | F: %.2f"), CurrentSpeed, CurrentFatigue);

				// On décale le texte un peu plus haut que la sphère pour que ce soit lisible
				FVector TextPos = Pos + FVector(0.f, 0.f, 60.f);

				// Couleur du texte : Blanc par défaut, mais passe en Rouge si fatigué ou figé pour attirer l'œil
				FColor TextColor = FColor::White;
				if (CurrentFatigue > 0.75f) TextColor = FColor::Orange;
				if (CurrentSpeed < 15.f && S.CurrentBehaviorState != EBoidState::Sleeping) TextColor = FColor::Red;

				// Dessin du texte dans le monde (Dure 1 frame, s'actualise en continu)
				DrawDebugString(World, TextPos, DebugText, nullptr, TextColor, 0.0f, /*bDrawShadow=*/true, 1.2f);
			}

			if (DebugMode == 2 || DebugMode == 3)
			{
				
				// =========================================================================
				// --- DESSIN DES LIGNES ET FLÈCHES DE CIBLE DYNAMIQUES ---
				// =========================================================================
				
				// 1. SI LE BOID CHERCHE DE LA NOURRITURE (FORAGING)
				if (S.CurrentBehaviorState == EBoidState::Foraging)
				{
					const FBoidGenome& G = Gen[It].Genome;
					
					const float Need = 1.f - FMath::Clamp(S.CurrentHunger / Evo::MaxHunger(G), 0.f, 1.f);
					const FVector BasePos = Xf[It].GetTransform().GetLocation(); // Position sans le Lift de debug

					// Recherche pour les herbivores (Plantes -> Jaune)
					if (Evo::CanEatPlants(G) && Need > 0.f)
					{
						FGridFood Food;
						if (Grid->FindNearestFood(BasePos, Radius, EFoodType::Plant, Food))
						{
							FVector FoodDebugPos = Food.Position + FVector(0, 0, Evo::DebugZLift);
							// Ligne et flèche Jaune vers la plante
							DrawDebugLine(World, Pos, FoodDebugPos, FColor::Yellow, false, -1.f, 0, 2.0f);
							DrawDebugDirectionalArrow(World, Pos, FoodDebugPos, 60.f, FColor::Yellow, false, -1.f, 0, 3.0f);
						}
					}

					// Recherche pour les carnivores/nécrophages (Carcasses -> Orange)
					if (Evo::CanHunt(G) && Need > 0.f)
					{
						FGridFood Carcass;
						if (Grid->FindNearestFood(BasePos, Radius, EFoodType::Carcass, Carcass))
						{
							FVector CarcassDebugPos = Carcass.Position + FVector(0, 0, Evo::DebugZLift);
							// Ligne et flèche Orange vers la carcasse
							DrawDebugLine(World, Pos, CarcassDebugPos, FColor::Orange, false, -1.f, 0, 2.0f);
							DrawDebugDirectionalArrow(World, Pos, CarcassDebugPos, 60.f, FColor::Orange, false, -1.f, 0, 3.0f);
						}
					}

					// --- C. ATTAQUE / TRAQUE D'UNE PROIE VIVANTE (Version Optimisée O(1)) ---
					if (Evo::CanHunt(G) && S.bDebugHasPrey)
					{
						FVector PreyDebugPos = S.LastTargetPreyPos + FVector(0, 0, Evo::DebugZLift);

						// Ligne Rouge vif très épaisse pour symboliser l'agression
						DrawDebugLine(World, Pos, PreyDebugPos, FColor::Red, false, -1.f, 0, 3.0f);
						DrawDebugDirectionalArrow(World, Pos, PreyDebugPos, 80.f, FColor::Red, false, -1.f, 0, 4.0f);
					}
				}
			
				// 2. SI LE BOID CHERCHE SON PARTENAIRE (MATING)
				else if (S.CurrentBehaviorState == EBoidState::Mating && S.TargetPartner.IsValid() && EntityManager.IsEntityValid(S.TargetPartner))
				{
					const FTransformFragment* PartnerXf = EntityManager.GetFragmentDataPtr<FTransformFragment>(S.TargetPartner);
					if (PartnerXf)
					{
						FVector PartnerPos = PartnerXf->GetTransform().GetLocation() + FVector(0, 0, Evo::DebugZLift);
						// Ligne Rose (Magenta) épaisse reliée au partenaire
						DrawDebugLine(World, Pos, PartnerPos, FColor::Magenta, false, -1.f, 0, 2.5f);
					}
				}
			}

			if (DebugMode == 3)
			{
				DrawDebugPoint(World, Pos, 25.f, SpeciesColor, false, -1.f, 0);
			}
			
			if (DebugMode == 4)
			{
				// =========================================================================
				// AUTRES DESSINS (Soumis à la touche B et à l'échantillonnage / Stride)
				// =========================================================================
				

				// Application du filtre d'échantillonnage uniquement pour alléger les gros cercles
				if (Context.GetEntity(It).Index % Evo::DebugSampleStride != 0)
				{
					continue;
				}
				
				// Bright marker dot at the boid so it's easy to locate.
				//DrawDebugPoint(World, Pos, 9.f, SpeciesColor, false, -1.f, 0);

				// Flèche de direction colorée par son espèce
				if (V.SizeSquared() > 1.f)
				{
					const FVector End = Pos + V.GetSafeNormal() * FMath::Max(200.f, Radius * 0.3f);
					DrawDebugDirectionalArrow(World, Pos, End, 120.f, SpeciesColor, false, -1.f, 0, Evo::DebugArrowThickness);
				}

				// Cercle de perception (Cœur de l'état + bordure fine de l'espèce)
				DrawDebugCircle(World, Pos, Radius, 32, BehaviorColor, false, -1.f, 0, Evo::DebugCircleThickness,
					FVector(1, 0, 0), FVector(0, 1, 0), false);
				
				// Un deuxième cercle très fin pour rappeler sa couleur d'origine/espèce
				DrawDebugCircle(World, Pos, Radius * 0.96f, 32, SpeciesColor, false, -1.f, 0, Evo::DebugCircleThickness * 0.4f,
					FVector(1, 0, 0), FVector(0, 1, 0), false);
			}
		}
	});
}
