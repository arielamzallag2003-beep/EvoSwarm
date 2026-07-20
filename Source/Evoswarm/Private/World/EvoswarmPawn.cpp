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
		
		PlayerInputComponent->BindKey(EKeys::NumPadZero, IE_Pressed, this, &AEvoswarmPawn::SetDebugMode0);
		PlayerInputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &AEvoswarmPawn::SetDebugMode1);
		PlayerInputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &AEvoswarmPawn::SetDebugMode2);
		PlayerInputComponent->BindKey(EKeys::NumPadThree, IE_Pressed, this, &AEvoswarmPawn::SetDebugMode3);
		PlayerInputComponent->BindKey(EKeys::NumPadFour, IE_Pressed, this, &AEvoswarmPawn::SetDebugMode4);
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

void AEvoswarmPawn::ApplyDebugMode(int32 Mode)
{
	if (UWorld* World = GetWorld())
	{
		if (UEvoswarmSimSubsystem* Sim = World->GetSubsystem<UEvoswarmSimSubsystem>())
		{
			Sim->SetDebugMode(Mode);
		}
	}
}

void AEvoswarmPawn::SetDebugMode0() { ApplyDebugMode(0); }
void AEvoswarmPawn::SetDebugMode1() { ApplyDebugMode(1); }
void AEvoswarmPawn::SetDebugMode2() { ApplyDebugMode(2); }
void AEvoswarmPawn::SetDebugMode3() { ApplyDebugMode(3); }
void AEvoswarmPawn::SetDebugMode4() { ApplyDebugMode(4); }