// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"
#include "EvoswarmSimSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace
{
	// Push transforms to an ISM. Fast path (no buffer realloc) when the instance count is
	// unchanged since last frame — only fully rebuilds when boids were born/died/eaten.
	void SyncInstances(UInstancedStaticMeshComponent* ISM, const TArray<FTransform>& Transforms)
	{
		if (!ISM)
		{
			return;
		}
		if (Transforms.Num() == 0 && ISM->GetInstanceCount() == 0)
		{
			return; // nothing to do (common for sparsely-populated appearance buckets)
		}
		if (Transforms.Num() > 0 && ISM->GetInstanceCount() == Transforms.Num())
		{
			ISM->BatchUpdateInstancesTransforms(0, Transforms, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);
		}
		else
		{
			ISM->ClearInstances();
			if (Transforms.Num() > 0)
			{
				ISM->AddInstances(Transforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
			}
		}
	}

	/**
	 * Procedural gait: bob, sway, pitch and squash driven by the creature's own locomotion
	 * phase, with every amplitude gated by normalised speed — so a walk is subtle and a sprint
	 * is emphatic from a single formula. Genome and life state shape the motion, so a heavy
	 * armoured creature plods, a wounded one limps, and a starving one staggers.
	 *
	 * IMPORTANT: this mutates ONLY the caller's local FTransform copy. Nothing here is written
	 * back into any fragment, so the gait can never perturb the simulation.
	 */
	void ApplyGait(FTransform& T, const FBoidGenome& G, const FBoidStateFragment& S,
		float Speed, float SpeciesStride, float Dt, float WorldTime)
	{
		const FVector Size = T.GetScale3D();

		// --- Asleep: deep, slow breathing and nothing else. --------------------------------
		// The movement processor zeroes velocity for sleepers, so this must be checked by state
		// rather than by speed — otherwise a sleeping creature is indistinguishable from one
		// that has merely stopped.
		if (S.CurrentBehaviorState == EBoidState::Sleeping)
		{
			const float Breathe = 1.f + Evo::GaitSleepBreatheAmp
				* FMath::Sin(WorldTime * Evo::GaitSleepBreatheFreq + S.WanderAngle);
			T.SetScale3D(FVector(Size.X, Size.Y, Size.Z * Breathe));
			return;
		}

		const float SpeedNorm = FMath::Clamp(Speed / FMath::Max(1.f, Evo::RunSpeed(G)), 0.f, 1.f);

		// --- Awake but stationary: shallow, quicker breathing, no footfalls. ---------------
		// WanderAngle is already per-individual and drifts slowly, so it doubles as a free
		// desynchronisation seed — without it every idle creature would breathe in unison.
		if (SpeedNorm < Evo::GaitIdleSpeedFrac)
		{
			const float Breathe = 1.f + Evo::GaitBreatheAmp
				* FMath::Sin(WorldTime * Evo::GaitBreatheFreq + S.WanderAngle);
			T.SetScale3D(Size * Breathe);
			return;
		}

		// --- Aliasing guard ----------------------------------------------------------------
		// If the cycle advances too far in one rendered frame (time dilation, or a frame spike)
		// the gait strobes like a wagon wheel, which reads far worse than no gait at all.
		// Fade the amplitude to zero and hold a neutral pose instead.
		const float Stride = FMath::Max(1.f, Evo::StrideLength(G) * SpeciesStride);
		const float PhaseStep = Speed * Dt * PI / Stride;
		const float Half = Evo::GaitMaxPhaseStep * 0.5f;
		const float Fade = 1.f - FMath::Clamp((PhaseStep - Half) / Half, 0.f, 1.f);
		if (Fade <= 0.f)
		{
			return;
		}

		const float P = S.GaitPhase;
		const float SinP = FMath::Sin(P);
		const float Sin2P = FMath::Sin(2.f * P);
		const float Cos2P = FMath::Cos(2.f * P);

		// Heavier creatures bounce less — the mass reads in the motion, not just the silhouette.
		const float Heft = 1.f / (1.f + Evo::GaitBobFromBiomass * G.Get(EBoidStat::Biomass));
		const float Amp = SpeedNorm * Fade * Heft;

		// --- Wounded: one leg gives, so alternate footfalls dip further than the other. -----
		const float MaxHP = Evo::MaxHP(G);
		const float HPFrac = (MaxHP > 0.f) ? FMath::Clamp(S.CurrentHP / MaxHP, 0.f, 1.f) : 1.f;
		const float Limp = (HPFrac < Evo::GaitLimpHPFraction)
			? Evo::GaitLimpAmount * (1.f - HPFrac / Evo::GaitLimpHPFraction)
			: 0.f;
		// Weakens only the positive half-cycle — that asymmetry is what reads as a limp.
		const float FootWeight = 1.f - Limp * FMath::Max(0.f, SinP);

		// --- Starving: an unsteady roll, deliberately off-beat from the footfalls. ----------
		const float MaxHunger = Evo::MaxHunger(G);
		const float Starve = (MaxHunger > 0.f)
			? 1.f - FMath::Clamp(S.CurrentHunger / MaxHunger, 0.f, 1.f)
			: 0.f;
		const float Stagger = Starve * Starve * Evo::GaitStaggerAmpDeg
			* FMath::Sin(WorldTime * Evo::GaitStaggerFreq + S.WanderAngle);

		// --- Vertical bounce: |sin| gives two footfalls per cycle. --------------------------
		const float Bob = Evo::GaitBobAmp * Amp * FootWeight * FMath::Abs(SinP) * Size.Z;
		T.SetLocation(T.GetLocation() + FVector(0.f, 0.f, Bob));

		// --- Body sway ----------------------------------------------------------------------
		// POST-multiplied, so the sway stays LOCAL to the terrain-aligned facing the movement
		// processor already built. Pre-multiplying would swing the creature around the world
		// axes instead, which looks completely wrong on slopes.
		const float RollDeg = Evo::GaitRollAmpDeg * Amp * SinP + Stagger;
		const float PitchDeg = Evo::GaitPitchAmpDeg * Amp * FootWeight * Cos2P;
		T.SetRotation(T.GetRotation() * FQuat(FRotator(PitchDeg, 0.f, RollDeg)));

		// --- Squash and stretch, roughly volume preserving. ---------------------------------
		const float Sq = Evo::GaitSquash * Amp * Sin2P;
		T.SetScale3D(FVector(Size.X * (1.f + Sq * 0.5f),
			Size.Y * (1.f + Sq * 0.5f),
			Size.Z * (1.f - Sq)));
	}
}

UBoidRenderProcessor::UBoidRenderProcessor()
	: EntityQuery(*this)
	, FoodQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true; // touches scene components
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidMovementProcessor"));
}

void UBoidRenderProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);
	// Gait inputs: speed (amplitude), life state (phase / sleep / limp / stagger), species (stride).
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSharedRequirement<FBoidSpeciesSharedFragment>(EMassFragmentAccess::ReadOnly);

	FoodQuery.AddTagRequirement<FFoodTag>(EMassFragmentPresence::All);
	FoodQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	FoodQuery.AddRequirement<FFoodFragment>(EMassFragmentAccess::ReadOnly);
}

void UBoidRenderProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}

	// Shared clock for the phase-independent cosmetics (breathing, starvation wobble).
	const float WorldTime = World->GetTimeSeconds();

	// --- Boids: binned by diet (colour) with per-individual size (from the genome) ---
	const int32 NumBuckets = Sim->NumBoidBuckets();
	PerBucketTransforms.SetNum(NumBuckets);
	for (TArray<FTransform>& Arr : PerBucketTransforms)
	{
		Arr.Reset();
	}

	EntityQuery.ForEachEntityChunk(Context, [this, NumBuckets, WorldTime](FMassExecutionContext& Context)
		{
			const float Dt = Context.GetDeltaTimeSeconds();
			const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
			const TConstArrayView<FMassVelocityFragment> Vel = Context.GetFragmentView<FMassVelocityFragment>();
			const TConstArrayView<FBoidStateFragment> State = Context.GetFragmentView<FBoidStateFragment>();

			// One species per chunk, so the stride multiplier is hoisted out of the loop.
			const FBoidSpeciesSharedFragment& Species = Context.GetSharedFragment<FBoidSpeciesSharedFragment>();
			const float SpeciesStride = Evo::SpeciesGaitScale(Species.SpeciesIndex);

			for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
			{
				const FBoidGenome& G = Gen[It].Genome;
				const int32 Bucket = Evo::AppearanceBucket(G);
				if (Bucket < 0 || Bucket >= NumBuckets)
				{
					continue;
				}
				FTransform T = Xf[It].GetTransform();
				T.SetScale3D(Evo::BodyScale(G)); // bulk from HP, length from speed, width from armour
				ApplyGait(T, G, State[It], Vel[It].Value.Size2D(), SpeciesStride, Dt, WorldTime);
				PerBucketTransforms[Bucket].Add(T);
			}
		});

	for (int32 Bucket = 0; Bucket < NumBuckets; ++Bucket)
	{
		SyncInstances(Sim->GetBoidBucketISM(Bucket), PerBucketTransforms[Bucket]);
	}

	// --- Food: plants and carcasses into their own ISMs ---
	FoodTransforms.Reset();
	CarcassTransforms.Reset();
	FoodQuery.ForEachEntityChunk(Context, [this](FMassExecutionContext& Context)
		{
			const TConstArrayView<FTransformFragment> Xf = Context.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FFoodFragment> Food = Context.GetFragmentView<FFoodFragment>();
			for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
			{
				if (Food[It].Type == EFoodType::Carcass)
				{
					FTransform T = Xf[It].GetTransform();
					T.SetRotation(Evo::MeshStandUp()); // imported meshes are Y-up
					T.SetScale3D(FVector(Evo::CarcassMeshScale));
					CarcassTransforms.Add(T);
				}
				else
				{
					FTransform T = Xf[It].GetTransform();
					T.SetRotation(Evo::MeshStandUp());
					T.SetScale3D(FVector(Evo::FoodMeshScale));
					FoodTransforms.Add(T);
				}
			}
		});

	SyncInstances(Sim->GetFoodISM(), FoodTransforms);
	SyncInstances(Sim->GetCarcassISM(), CarcassTransforms);
}