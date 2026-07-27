// Copyright Evoswarm.
//
// A free-flying spectator pawn (ZQSD via input config) with:
// - B key:       toggle debug visualisation
// - Numpad 0-4:  select the debug overlay mode
// - F key:       lock/unlock crosshair inspect
// - G key:       open/close the analytics dashboard
// - Tab:         next dashboard page
// - [ / ]:       cycle the current page's selection (charted trait, or scatter axis pair)
// - K key:       export the recorded run to CSV
// - Tick:        center-screen ray -> terrain hit -> grid query -> find the nearest boid,
//                read its live fragments into the sim subsystem's inspect state, and
//                draw a selection ring around it.

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
	void ToggleSelect();

	// Analytics dashboard. These only mutate view state on the sim subsystem, exactly like the
	// debug-mode handlers below - the Slate panel reads it and repaints itself.
	void ToggleAnalytics();
	void NextAnalyticsPage();
	void PrevAnalyticsSelection();
	void NextAnalyticsSelection();
	void ExportAnalyticsCsv();

	// Handlers pour changer le mode de debug (numpad 0-4).
	void SetDebugMode0();
	void SetDebugMode1();
	void SetDebugMode2();
	void SetDebugMode3();
	void SetDebugMode4();

	// Helper pour envoyer le mode au subsystem.
	void ApplyDebugMode(int32 Mode);

	/** Shared accessor for the handlers above; null while the world is tearing down. */
	class UEvoswarmSimSubsystem* GetSim() const;
};