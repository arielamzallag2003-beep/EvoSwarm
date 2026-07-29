// Copyright Evoswarm.
//
// The analytics dashboard: a full-viewport Slate overlay, opened with G, showing four pages
// over the run's recorded history.
//
//   TRAITS     one stat, every species, mean +/- 1 sd over the whole run
//   POPULATION stacked population area with birth / death rates on a second axis
//   SCATTER    individual genomes in 2D trait space, where clades separate visibly
//   FOOD WEB   who eats what, arrows sized by real energy throughput
//
// Page and selection live on the sim subsystem (the pawn's keys write them, this reads them),
// which keeps the widget stateless and means it survives being collapsed. Every chart is a
// hand-painted SLeafWidget - still no UMG, still no assets.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UEvoswarmSimSubsystem;

class EVOSWARM_API SEvoswarmAnalytics : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoswarmAnalytics) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	/** Bound attribute (not SetVisibility): a collapsed widget never ticks, so it could never
	 *  reopen itself. The parent evaluates this during arrangement every frame instead. */
	EVisibility ContentVisibility() const;

	int32 ActivePageIndex() const;

	void Refresh();

	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
	float RefreshTimer = 0.f;

	FText PageTitleText;
	FText SelectionText;
	FText SubtitleText;
	FText FooterText;
};