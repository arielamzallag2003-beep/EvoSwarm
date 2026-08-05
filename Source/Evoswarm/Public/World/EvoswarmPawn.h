// Copyright Evoswarm.
//
// A free-flying spectator pawn (ZQSD via input config) that adds a key binding to toggle
// the debug visualisation. Otherwise identical to ADefaultPawn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "EvoswarmPawn.generated.h"

UCLASS()
class EVOSWARM_API AEvoswarmPawn : public ADefaultPawn
{
	GENERATED_BODY()

public:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void ToggleDebug();
	
	// Handlers pour changer le mode de débug
	void SetDebugMode0();
	void SetDebugMode1();
	void SetDebugMode2();
	void SetDebugMode3();
	void SetDebugMode4();

	// Helper pour envoyer le mode au subsystem
	void ApplyDebugMode(int32 Mode);
};
