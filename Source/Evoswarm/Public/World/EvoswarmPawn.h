// Copyright Evoswarm.
//
// A free-flying spectator pawn (ZQSD via input config) with:
// - B key: toggle debug visualisation
// - F key: lock/unlock crosshair inspect
// - Tick:  center-screen ray -> terrain hit -> grid query -> find the nearest boid,
//        read its live fragments into the sim subsystem's inspect state, and
//        draw a selection ring around it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "EvoswarmPawn.generated.h"

UCLASS()
class EVOSWARM_API AEvoswarmPawn : public ADefaultPawn
{
	GENERATED_BODY()

public:
	AEvoswarmPawn();

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaTime) override;

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
	void ToggleSelect();
};
