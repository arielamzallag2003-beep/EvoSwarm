// Copyright Evoswarm.

#include "EvoswarmPawn.h"
#include "Components/InputComponent.h"
#include "EvoswarmSimSubsystem.h"

void AEvoswarmPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent); // keep the default fly bindings (ZQSD via config)
	if (PlayerInputComponent)
	{
		PlayerInputComponent->BindAction(TEXT("ToggleDebug"), IE_Pressed, this, &AEvoswarmPawn::ToggleDebug);
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
