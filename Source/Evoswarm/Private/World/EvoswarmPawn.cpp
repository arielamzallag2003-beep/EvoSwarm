// Copyright Evoswarm.

#include "EvoswarmPawn.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "EvoswarmSimSubsystem.h"
#include "BoidGridSubsystem.h"
#include "BoidFragments.h"
#include "BoidStats.h"
#include "EvoswarmTuning.h"
#include "EvoswarmTerrain.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// The world carries no collision geometry at all: the procedural terrain, the flora
	// props and the boid/food ISMs are every one of them NoCollision, so a physics trace
	// from the camera can never report a hit. Intersect the analytic height field instead
	// -- same function the boids already walk on, and free compared with cooking collision.
	bool RayMarchTerrain(const FVector& Origin, const FVector& Dir, FVector& OutHit)
	{
		constexpr float MaxRange = 80000.f;   // give up past this distance
		constexpr float BaseStep = 120.f;     // step at the camera, grown with distance
		constexpr int32 MaxSteps = 512;
		constexpr int32 RefineSteps = 12;     // bisections once we straddle the surface

		auto GapAt = [](const FVector& P) { return P.Z - Evo::SurfaceZ(P.X, P.Y); };

		if (GapAt(Origin) <= 0.f)
		{
			OutHit = Origin; // camera is already below the surface
			return true;
		}

		float PrevT = 0.f;
		float T = 0.f;
		for (int32 Step = 0; Step < MaxSteps && T < MaxRange; ++Step)
		{
			T += BaseStep * (1.f + T / 6000.f);
			if (GapAt(Origin + Dir * T) <= 0.f)
			{
				float Lo = PrevT; // above the surface
				float Hi = T;     // below it
				for (int32 I = 0; I < RefineSteps; ++I)
				{
					const float Mid = 0.5f * (Lo + Hi);
					((GapAt(Origin + Dir * Mid) > 0.f) ? Lo : Hi) = Mid;
				}
				OutHit = Origin + Dir * (0.5f * (Lo + Hi));
				return true;
			}
			PrevT = T;
		}
		return false; // pointing at the sky
	}

	// Picking tolerance in world units at a given distance along the ray: a fixed ~1.5 deg
	// cone, so the grab radius stays a constant size on screen however far away you are.
	float PickTolerance(float DistanceAlongRay)
	{
		return FMath::Clamp(DistanceAlongRay * 0.026f, 120.f, 900.f);
	}
}

AEvoswarmPawn::AEvoswarmPawn()
{
	// Tick after all Mass processors (PrePhysics) have finished for this frame,
	// so fragment reads are safe and the grid is fully populated.
	PrimaryActorTick.TickGroup = TG_DuringPhysics;
	PrimaryActorTick.bCanEverTick = true;
}

void AEvoswarmPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent); // keep the default fly bindings

	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindAction(TEXT("ToggleDebug"), IE_Pressed, this, &AEvoswarmPawn::ToggleDebug);
		PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AEvoswarmPawn::ToggleSelect);
	}
}

void AEvoswarmPawn::ToggleDebug()
{
	if (UWorld* World = GetWorld())
	{
		if (UEvoswarmSimSubsystem* Sim = World->GetSubsystem<UEvoswarmSimSubsystem>())
		{
			Sim->ToggleDebugDraw();
		}
	}
}

void AEvoswarmPawn::ToggleSelect()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UEvoswarmSimSubsystem* Sim = World->GetSubsystem<UEvoswarmSimSubsystem>();
	if (!Sim)
	{
		return;
	}

	FBoidInspectState& Ins = Sim->GetInspectMutable();

	if (Ins.bDeceased)
	{
		// Dismiss the deceased panel.
		Ins = FBoidInspectState();
		return;
	}
	if (Ins.bLocked)
	{
		// Unlock: go back to hover mode.
		Ins.bLocked = false;
		return;
	}
	if (Ins.bActive)
	{
		// Lock on whatever is currently under the crosshair.
		Ins.bLocked = true;
	}
}

void AEvoswarmPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UEvoswarmSimSubsystem* Sim = World->GetSubsystem<UEvoswarmSimSubsystem>();
	if (!Sim)
	{
		return;
	}

	FBoidInspectState& Ins = Sim->GetInspectMutable();

	// --- Entity-manager handle (for fragment reads + validity checks) ---
	UMassEntitySubsystem* MassSys = World->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager* EM = MassSys ? &MassSys->GetMutableEntityManager() : nullptr;

	// --- If locked, check whether the entity is still alive ---
	if (Ins.bLocked && Ins.Entity.IsValid() && !Ins.bDeceased)
	{
		if (!EM || !EM->IsEntityValid(Ins.Entity))
		{
			Ins.bDeceased = true;
			// Data stays frozen at last-known values; the HUD shows a "deceased" banner.
		}
	}

	// --- When not locked: center-screen ray -> terrain -> grid query ---
	if (!Ins.bLocked)
	{
		bool bFound = false;
		FGridAgent BestAgent;

		APlayerController* PC = Cast<APlayerController>(GetController());
		UBoidGridSubsystem* Grid = World->GetSubsystem<UBoidGridSubsystem>();

		int32 VX = 0, VY = 0;
		if (PC)
		{
			PC->GetViewportSize(VX, VY);
		}

		FVector RayOrigin, RayDir, Ground;
		if (PC && Grid && VX > 0 && VY > 0
			&& PC->DeprojectScreenPositionToWorld(VX * 0.5f, VY * 0.5f, RayOrigin, RayDir)
			&& RayMarchTerrain(RayOrigin, RayDir, Ground))
		{
			// Search a disc around the ground point, but score by distance to the RAY, not
			// to the ground point: from an oblique camera those are very different answers.
			// Among everything inside the cone, the nearest to the camera is the one you see.
			const float GroundDist = FVector::Dist(RayOrigin, Ground);
			const float SearchRadius = PickTolerance(GroundDist) + 600.f;
			float BestAlong = TNumericLimits<float>::Max();

			Grid->QueryAgents(Ground, SearchRadius, [&](const FGridAgent& Agent)
				{
					const FVector ToAgent = Agent.Position - RayOrigin;
					const float Along = FVector::DotProduct(ToAgent, RayDir);
					if (Along <= 0.f || Along >= BestAlong)
					{
						return; // behind the camera, or already beaten by a closer candidate
					}

					const float PerpSq = FMath::Max(0.f, ToAgent.SizeSquared() - Along * Along);
					const float Tol = PickTolerance(Along);
					if (PerpSq > Tol * Tol)
					{
						return; // outside the crosshair cone
					}

					BestAlong = Along;
					BestAgent = Agent;
					bFound = true;
				});
		}

		if (bFound)
		{
			Ins.bActive = true;
			Ins.bDeceased = false;
			Ins.Entity = BestAgent.Entity;
			Ins.SpeciesIndex = BestAgent.SpeciesIndex;
			Ins.Position = BestAgent.Position;
		}
		else
		{
			// Nothing under the crosshair (sky, empty ground, or no controller yet).
			Ins.bActive = false;
			Ins.Entity = FMassEntityHandle();
		}
	}

	// --- Read live fragment data from the entity (if still alive) ---
	if (Ins.bActive && Ins.Entity.IsValid() && !Ins.bDeceased && EM && EM->IsEntityValid(Ins.Entity))
	{
		const FTransformFragment& Xf = EM->GetFragmentDataChecked<FTransformFragment>(Ins.Entity);
		Ins.Position = Xf.GetTransform().GetLocation();

		const FBoidGenomeFragment& Gen = EM->GetFragmentDataChecked<FBoidGenomeFragment>(Ins.Entity);
		Ins.Genome = Gen.Genome;
		Ins.MaxHP = Evo::MaxHP(Gen.Genome);
		Ins.MaxStam = Evo::MaxStamina(Gen.Genome);
		Ins.MaxHunger = Evo::MaxHunger(Gen.Genome);
		Ins.Lifespan = Evo::Lifespan(Gen.Genome);

		const FBoidStateFragment& St = EM->GetFragmentDataChecked<FBoidStateFragment>(Ins.Entity);
		Ins.HP = St.CurrentHP;
		Ins.Stam = St.CurrentStamina;
		Ins.Hunger = St.CurrentHunger;
		Ins.Age = St.Age;
		Ins.Generation = St.Generation;
		Ins.ReproCount = St.ReproductionCount;
		Ins.Adrenaline = St.Adrenaline;
		Ins.ReproCooldown = St.ReproCooldown;
		Ins.AttackCooldown = St.AttackCooldown;
	}
	else if (Ins.bActive && !Ins.bDeceased)
	{
		// Entity disappeared while hovering (not locked): just drop the selection.
		if (!EM || !Ins.Entity.IsValid() || !EM->IsEntityValid(Ins.Entity))
		{
			if (Ins.bLocked)
			{
				Ins.bDeceased = true;
			}
			else
			{
				Ins.bActive = false;
				Ins.Entity = FMassEntityHandle();
			}
		}
	}

	// --- Selection ring (drawn in world space every frame) ---
	if (Ins.bActive)
	{
		const FVector RingPos = Ins.Position + FVector(0.f, 0.f, Evo::DebugZLift);
		const FColor RingColor = Ins.bDeceased
			? FColor(180, 50, 50, 200)
			: (Ins.bLocked ? FColor(255, 220, 60, 255) : FColor(200, 220, 255, 180));
		constexpr float RingRadius = 130.f;

		DrawDebugCircle(World, RingPos, RingRadius, 36, RingColor, false, -1.f, 0, 3.f,
			FVector(1, 0, 0), FVector(0, 1, 0), false);

		if (Ins.bLocked && !Ins.bDeceased)
		{
			// Double ring for emphasis when locked.
			DrawDebugCircle(World, RingPos, RingRadius * 0.82f, 36, RingColor, false, -1.f, 0, 1.5f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
		}

		if (Ins.bDeceased)
		{
			// Small cross for a deceased marker.
			constexpr float Arm = 35.f;
			const FColor XColor(200, 60, 60, 220);
			DrawDebugLine(World, RingPos + FVector(-Arm, -Arm, 0), RingPos + FVector(Arm, Arm, 0), XColor, false, -1.f, 0, 2.5f);
			DrawDebugLine(World, RingPos + FVector(-Arm, Arm, 0), RingPos + FVector(Arm, -Arm, 0), XColor, false, -1.f, 0, 2.5f);
		}
	}
}