// Copyright Evoswarm.
//
// A lightweight canvas HUD that reads the sim subsystem's live stats and draws a
// per-species panel: population bar + population-averaged genome values, plus global
// totals and the food count. No UMG assets required.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EvoswarmHUD.generated.h"

UCLASS()
class EVOSWARM_API AEvoswarmHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
