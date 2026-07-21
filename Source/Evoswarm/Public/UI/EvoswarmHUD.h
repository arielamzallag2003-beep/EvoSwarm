// Copyright Evoswarm.
//
// The HUD actor. It no longer draws anything itself: it constructs the Slate ecosystem
// panel (SEvoswarmHUD) once, adds it to the viewport, and removes it on teardown. All the
// rendering, layout and ~10 Hz refresh live in the Slate widget, which reads the sim
// subsystem's live-stats data layer directly. No UMG assets required.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EvoswarmHUD.generated.h"

class SEvoswarmHUD;

UCLASS()
class EVOSWARM_API AEvoswarmHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** The Slate panel, added to the viewport for the lifetime of play. */
	TSharedPtr<SEvoswarmHUD> HudWidget;
};