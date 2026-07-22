// Copyright Evoswarm.
//
// The Slate ecosystem HUD. Retained-mode replacement for the old Canvas DrawHUD panel.
// Layout: a full-viewport SOverlay containing the left ecosystem panel, a right-side
// crosshair inspector (visible when a boid is under the cursor or locked), and a tiny
// center-screen dot as a crosshair reference.

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
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void BuildSpeciesRows();
	void RefreshGlobalBar(float DeltaTime);
	void RefreshEventFeed();

	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;

	TSharedPtr<SVerticalBox> SpeciesContainer;
	TSharedPtr<SScrollBox>   EventScroll;
	TSharedPtr<int32>        SharedPopMax;

	// Global-bar cached values.
	FText       SummaryText;
	FText       FpsText;
	FSlateColor FpsColor = FSlateColor(FLinearColor::White);

	bool  bRowsBuilt = false;
	int32 LastEventCount = -1;
	float RefreshTimer = 0.f;
	float SmoothedFps = 60.f;
};