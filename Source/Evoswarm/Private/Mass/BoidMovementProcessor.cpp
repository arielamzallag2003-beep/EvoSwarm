// Copyright Evoswarm.

#include "BoidProcessors.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"
#include "EvoswarmTerrain.h"

UBoidMovementProcessor::UBoidMovementProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionOrder.ExecuteAfter.Add(TEXT("BoidSteeringProcessor"));
}

void UBoidMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FBoidTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FBoidGenomeFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBoidStateFragment>(EMassFragmentAccess::ReadWrite);
}

void UBoidMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Context)
	{
		const float Dt = Context.GetDeltaTimeSeconds();
		const TArrayView<FTransformFragment> Xf = Context.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassVelocityFragment> Vel = Context.GetMutableFragmentView<FMassVelocityFragment>();
		const TArrayView<FMassForceFragment> Force = Context.GetMutableFragmentView<FMassForceFragment>();
		const TConstArrayView<FBoidGenomeFragment> Gen = Context.GetFragmentView<FBoidGenomeFragment>();
		const TArrayView<FBoidStateFragment> State = Context.GetMutableFragmentView<FBoidStateFragment>();

		for (FMassExecutionContext::FEntityIterator It = Context.CreateEntityIterator(); It; ++It)
		{
			const FBoidGenome& G = Gen[It].Genome;
			FBoidStateFragment& S = State[It];
			FVector Loc = Xf[It].GetTransform().GetLocation();

			// Sprint when strongly driven (fleeing / chasing) and stamina remains.
			const bool bWantSprint = Force[It].Value.SizeSquared() > FMath::Square(0.7f * Evo::MaxSteerAccel);
			float MaxSpeed = Evo::WalkSpeed(G);
			if (bWantSprint && S.CurrentStamina > 0.f)
			{
				MaxSpeed = Evo::RunSpeed(G);
				S.CurrentStamina = FMath::Max(0.f, S.CurrentStamina - Evo::StaminaDrainPerSec * Dt);
			}
			else
			{
				S.CurrentStamina = FMath::Min(Evo::MaxStamina(G), S.CurrentStamina + Evo::StaminaRegenPerSec * Dt);
			}

			// Biome and adrenaline scale top speed; uphill travel is slower.
			const FBiomeParams Biome = Evo::GetBiomeParams(Evo::BiomeAt(Loc.X, Loc.Y));
			float SpeedScale = Biome.SpeedMultiplier;
			if (S.Adrenaline > 0.f)
			{
				SpeedScale *= Evo::AdrenalineSpeedMult;
			}
			// Swimming: over water, speed depends on aquatic adaptation — a strong swimmer barely
			// slows, a poor one struggles. Either way they can cross; it just costs the weak ones.
			if (Evo::TerrainHeight(Loc.X, Loc.Y) < Evo::SeaLevel)
			{
				SpeedScale *= FMath::Lerp(Evo::WaterWadeSpeedMin, Evo::WaterWadeSpeedMax, Evo::AquaticAdaptation(G));
			}

			// Desired velocity from the steering force this frame.
			FVector Target = Vel[It].Value + Force[It].Value * Dt;
			Target.Z = 0.f;

			if (!Target.IsNearlyZero())
			{
				const FVector Dir = Target.GetSafeNormal();
				const float E = 50.f;
				const float DHdx = (Evo::TerrainHeight(Loc.X + E, Loc.Y) - Evo::TerrainHeight(Loc.X - E, Loc.Y)) / (2.f * E);
				const float DHdy = (Evo::TerrainHeight(Loc.X, Loc.Y + E) - Evo::TerrainHeight(Loc.X, Loc.Y - E)) / (2.f * E);
				const float Uphill = FMath::Max(0.f, Dir.X * DHdx + Dir.Y * DHdy); // rise per unit run
				SpeedScale *= FMath::Clamp(1.f - Uphill * Evo::SlopeSpeedPenalty, 1.f - Evo::SlopeSpeedPenalty, 1.f);
			}

			MaxSpeed *= FMath::Max(0.1f, SpeedScale);
			Target = Target.GetClampedToMaxSize(MaxSpeed);

			// Low-pass the velocity so steering changes ease in rather than snapping (less shaky).
			FVector V = FMath::VInterpTo(Vel[It].Value, Target, Dt, Evo::ForceSmoothing);
			V.Z = 0.f;
			Vel[It].Value = V;

			Loc.X = FMath::Clamp(Loc.X + V.X * Dt, -Evo::ArenaHalfExtent, Evo::ArenaHalfExtent);
			Loc.Y = FMath::Clamp(Loc.Y + V.Y * Dt, -Evo::ArenaHalfExtent, Evo::ArenaHalfExtent);
			Loc.Z = Evo::SurfaceZ(Loc.X, Loc.Y) + Evo::GroundOffset; // walk on the surface (skim over water)

			FTransform& T = Xf[It].GetMutableTransform();
			T.SetLocation(Loc);
			if (V.SizeSquared() > 1.f)
			{
				// Smoothly turn to face travel, tilted to the terrain — no snapping.
				const FVector Up = Evo::TerrainNormal(Loc.X, Loc.Y);
				const FQuat TargetRot = FRotationMatrix::MakeFromXZ(V.GetSafeNormal(), Up).ToQuat();
				T.SetRotation(FMath::QInterpTo(T.GetRotation(), TargetRot, Dt, Evo::FacingInterpSpeed));
			}

			// Consumed this frame; steering rewrites it next frame.
			Force[It].Value = FVector::ZeroVector;
		}
	});
}
