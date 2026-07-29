// Copyright Evoswarm.
//
// The HUD actor. The rich per-species stats panel is now theo's retained-mode Slate widget
// (SEvoswarmHUD), constructed once in BeginPlay and added to the viewport. This actor keeps
// a tiny Canvas overlay of its own for the two things the Slate panel doesn't cover: the
// build stamp and the day/night clock (both top-right). H toggles the Slate panel.

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

	/** Tiny top-right overlay: build stamp + day/night clock (info the Slate panel omits). */
	virtual void DrawHUD() override;

	/** Show/hide theo's Slate stats panel. Bound to the H key. */
	void TogglePanel();

private:
	/** theo's Slate ecosystem panel, added to the viewport for the lifetime of play. */
	TSharedPtr<SEvoswarmHUD> HudWidget;

	bool bShowPanel = true;

	/** Module build timestamp ("build 2026-07-21 11:32") — read once from the DLL on disk,
	 *  so it can never lie about which binary the editor actually loaded. */
	FString BuildStamp;
};
