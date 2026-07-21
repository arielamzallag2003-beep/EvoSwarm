// Copyright Evoswarm.
//
// The Slate ecosystem HUD. This is the retained-mode replacement for the old Canvas
// DrawHUD panel: a fixed-width left panel with a global bar, a per-species row list
// (colour swatch, live sparkline, diet-gradient histogram, averaged genome, demographics),
// a colour/shape legend, and a scrollable event feed. It reads everything from the sim
// subsystem's live-stats data layer; it owns no simulation state of its own.
//
// Only this root widget is public. The per-species row and the custom-painted leaf widgets
// (sparkline, diet bar, gradient strip) live entirely inside the .cpp, since nothing outside
// the HUD constructs them directly.
//
// Refresh policy: text is rebuilt on a ~10 Hz throttle (see Tick) to avoid per-frame string
// churn; the custom-painted graphs read live every paint (drawing is cheap and stays smooth).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateColor.h"

class UEvoswarmSimSubsystem;
class SVerticalBox;
class SScrollBox;

class EVOSWARM_API SEvoswarmHUD : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoswarmHUD) {}
		/** The world's sim subsystem, source of all live stats and events. */
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget: throttled refresh of cached text + lazy row construction once the sim is ready.
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** Build one row per species into SpeciesContainer. Called once, when the sim first reports species. */
	void BuildSpeciesRows();

	/** Recompute the global-bar strings (population, food, generation, clock, FPS) + the shared sparkline scale. */
	void RefreshGlobalBar(float DeltaTime);

	/** Rebuild the event feed only when the event count changed, keeping it freely scrollable otherwise. */
	void RefreshEventFeed();

	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;

	// Layout anchors populated in Construct and filled in lazily.
	TSharedPtr<SVerticalBox> SpeciesContainer;
	TSharedPtr<SScrollBox>   EventScroll;

	/** Shared vertical scale for every species sparkline, so the curves are directly comparable. */
	TSharedPtr<int32> SharedPopMax;

	// Cached global-bar display values (rebuilt at ~10 Hz, read every frame by bound lambdas).
	FText       SummaryText;
	FText       FpsText;
	FSlateColor FpsColor = FSlateColor(FLinearColor::White);

	bool  bRowsBuilt = false;
	int32 LastEventCount = -1;
	float RefreshTimer = 0.f;
	float SmoothedFps = 60.f;
};