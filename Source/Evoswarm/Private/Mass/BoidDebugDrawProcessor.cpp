// Copyright Evoswarm.
//
// Overlay de debug. Le découpage des modes et le rendu reprennent la version de la
// soutenance (gros points d'état saturés + flèches de cible), avec deux différences
// assumées :
//   - la cible affichée vient du cache rempli par le steering, au lieu d'une requête
//     spatiale relancée par créature et par frame (c'est ce coût-là qui figeait l'éditeur) ;
//   - l'état « Hunting », ajouté depuis, a sa propre couleur, et les barres de vie/faim
//     apparaissent dans le mode 3.

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
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	/** Couleurs d'état : teintes pures, pour que le comportement se lise d'un coup d'œil. */
	FColor StateColor(EBoidState State)
	{
		switch (State)
		{
		case EBoidState::Wandering: return FColor::Green;
		case EBoidState::Foraging:  return FColor::Yellow;
		case EBoidState::Hunting:   return FColor::Orange;   // état ajouté par la refonte de la FSM
		case EBoidState::Fleeing:   return FColor::Red;
		case EBoidState::Mating:    return FColor::Magenta;
		case EBoidState::Sleeping:  return FColor::Black;
		default:                    return FColor::White;
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

	// 0 = rien
	// 1 = point d'état
	// 2 = point d'état + vitesse/fatigue + flèches de cible
	// 3 = point d'espèce + flèches de cible + barres vie/faim
	// 4 = cercles de perception + flèche de direction (échantillonné)
	const int32 Mode = Sim->GetDebugMode();
	if (Mode <= 0)
	{
		return;
	}

	const bool bStatePoint = (Mode == 1 || Mode == 2);
	const bool bSpeciesPoint = (Mode == 3);
	const bool bTargets = (Mode == 2 || Mode == 3);
	const bool bText = (Mode == 2);
	const bool bVitals = (Mode == 3);
	const bool bPerception = (Mode == 4);

	// Position caméra : sert uniquement à limiter le texte à ce qui est réellement lisible.
	FVector CamPos = FVector::ZeroVector;
	bool bHasCam = false;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			CamPos = PC->PlayerCameraManager->GetCameraLocation();
			bHasCam = true;
		}
	}

	EntityQuery.ForEachEntityChunk(Context, [World, CamPos, bHasCam, bStatePoint, bSpeciesPoint,
		bTargets, bText, bVitals, bPerception](FMassExecutionContext& Context)
		{
			const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
			const FColor SpeciesColor = Species.Color.ToFColor(true);
			const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FMassVelocityFragment> Vel = Context.GetFragmentView<FMassVelocityFragment>();
			const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
			const TConstArrayView<FBoidStateFragment> St = Context.GetFragmentView<FBoidStateFragment>();

			for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
			{
				// Seuls les cercles de perception sont échantillonnés : ce sont les seuls
				// dessins assez lourds et assez larges pour saturer l'écran.
				if (bPerception && (Context.GetEntity(It).Index % Evo::DebugSampleStride) != 0)
				{
					continue;
				}

				// Overlays relevés au-dessus du corps, pour qu'ils passent devant le terrain.
				const FVector Pos = Xf[It].GetTransform().GetLocation() + FVector(0, 0, Evo::DebugZLift);
				const FVector V = Vel[It].Value;
				const FBoidGenome& G = Gen[It].Genome;
				const FBoidStateFragment& S = St[It];
				const float Radius = Evo::PerceptionRadius(G);
				const FColor BehaviorColor = StateColor(S.CurrentBehaviorState);

				// --- Gros point : couleur d'état (1, 2) ou couleur d'espèce (3).
				if (bStatePoint)
				{
					DrawDebugPoint(World, Pos, Evo::DebugPointSize, BehaviorColor, false, -1.f, 0);
				}
				else if (bSpeciesPoint)
				{
					DrawDebugPoint(World, Pos, Evo::DebugPointSize, SpeciesColor, false, -1.f, 0);
				}

				// --- Ligne + flèche vers ce que la créature poursuit réellement. La position
				// vient du cache rempli par le steering : aucune requête spatiale ici.
				if (bTargets && S.DebugTargetKind != EBoidDebugTarget::None)
				{
					FColor TargetColor = FColor::White;
					float LineThickness = 2.f;
					float HeadSize = 60.f;
					float HeadThickness = 3.f;
					switch (S.DebugTargetKind)
					{
					case EBoidDebugTarget::Plant:   TargetColor = FColor::Yellow; break;
					case EBoidDebugTarget::Carcass: TargetColor = FColor::Orange; break;
					case EBoidDebugTarget::Prey:    TargetColor = FColor::Red;
						LineThickness = 3.f; HeadSize = 80.f; HeadThickness = 4.f; break;
					case EBoidDebugTarget::Mate:    TargetColor = FColor::Magenta;
						LineThickness = 2.5f; HeadThickness = 0.f; break; // partenaire : ligne seule
					default: break;
					}
					const FVector TargetPos = S.DebugTargetPos + FVector(0, 0, Evo::DebugZLift);
					DrawDebugLine(World, Pos, TargetPos, TargetColor, false, -1.f, 0, LineThickness);
					if (HeadThickness > 0.f)
					{
						DrawDebugDirectionalArrow(World, Pos, TargetPos, HeadSize, TargetColor, false, -1.f, 0, HeadThickness);
					}
				}

				// --- Vitesse / fatigue. La fatigue est DÉRIVÉE de l'endurance (le champ stocké
				// n'existe plus). Limité à la portée de lecture : au-delà c'était illisible.
				if (bText && (!bHasCam || FVector::DistSquared(Pos, CamPos) < Evo::DebugTextMaxDistSq))
				{
					const float Speed = V.Size();
					const float Fatigue = Evo::Fatigue(G, S.CurrentStamina);
					FColor TextColor = FColor::White;
					if (Fatigue > 0.75f) { TextColor = FColor::Orange; }
					if (Speed < 15.f && S.CurrentBehaviorState != EBoidState::Sleeping) { TextColor = FColor::Red; }
					DrawDebugString(World, Pos + FVector(0.f, 0.f, 60.f),
						FString::Printf(TEXT("V: %.1f | F: %.2f"), Speed, Fatigue),
						nullptr, TextColor, 0.f, /*bDrawShadow=*/true, 1.2f);
				}

				if (bVitals)
				{
					// Deux barres empilées : PV au-dessus (rouge -> vert), faim en dessous (ambre).
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

				if (bPerception)
				{
					// Flèche de direction à la couleur de l'espèce.
					if (V.SizeSquared() > 1.f)
					{
						const FVector End = Pos + V.GetSafeNormal() * FMath::Max(200.f, Radius * 0.3f);
						DrawDebugDirectionalArrow(World, Pos, End, 120.f, SpeciesColor, false, -1.f, 0, Evo::DebugArrowThickness);
					}

					// Cercle de perception : cœur à la couleur de l'état, liseré fin à celle de l'espèce.
					DrawDebugCircle(World, Pos, Radius, 32, BehaviorColor, false, -1.f, 0, Evo::DebugCircleThickness,
						FVector(1, 0, 0), FVector(0, 1, 0), false);
					DrawDebugCircle(World, Pos, Radius * 0.96f, 32, SpeciesColor, false, -1.f, 0, Evo::DebugCircleThickness * 0.4f,
						FVector(1, 0, 0), FVector(0, 1, 0), false);
				}
			}
		});
}
