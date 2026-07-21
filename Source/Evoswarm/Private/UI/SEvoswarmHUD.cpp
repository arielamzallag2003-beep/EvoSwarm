// Copyright Evoswarm.

#include "SEvoswarmHUD.h"
#include "EvoswarmSimSubsystem.h"
#include "EvoswarmTuning.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SLeafWidget.h"

#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Rendering/DrawElements.h"

#define LOCTEXT_NAMESPACE "Evoswarm"

// ---------------------------------------------------------------------------------------------
// Shared style helpers (code-only: no asset dependencies, no UMG).
// ---------------------------------------------------------------------------------------------
namespace
{
	/** A plain white fill brush; tinted per draw call. Self-contained so we depend on no style set. */
	const FSlateColorBrush GWhiteBrush(FLinearColor::White);

	// Panel palette (mirrors the old Canvas HUD so the visual identity is preserved).
	const FLinearColor CPanelBg(0.02f, 0.025f, 0.04f, 0.86f);
	const FLinearColor CWhite(0.92f, 0.94f, 0.97f, 1.f);
	const FLinearColor CGrey(0.65f, 0.68f, 0.72f, 1.f);
	const FLinearColor CBarBg(0.12f, 0.13f, 0.16f, 1.f);
	const FLinearColor CAccent(0.30f, 0.65f, 1.0f, 1.f);
	const FLinearColor CGood(0.55f, 0.85f, 0.55f, 1.f);
	const FLinearColor CBad(0.85f, 0.55f, 0.55f, 1.f);

	FSlateFontInfo FontTitle() { return FCoreStyle::GetDefaultFontStyle("Bold", 15); }
	FSlateFontInfo FontName() { return FCoreStyle::GetDefaultFontStyle("Bold", 11); }
	FSlateFontInfo FontBody() { return FCoreStyle::GetDefaultFontStyle("Regular", 9); }
	FSlateFontInfo FontSmall() { return FCoreStyle::GetDefaultFontStyle("Regular", 8); }

	/** Fixed panel width, matching the old 494 px Canvas panel. */
	constexpr float PanelWidth = 500.f;

	FText ClockText(float Seconds)
	{
		const int32 M = FMath::FloorToInt(Seconds / 60.f);
		const int32 S = FMath::FloorToInt(Seconds) % 60;
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), M, S));
	}

	/** A thin horizontal divider line (self-contained; no style-set dependency). */
	TSharedRef<SWidget> MakeRule(const FLinearColor& Color, float Height = 1.f)
	{
		return SNew(SBox).HeightOverride(Height)
			[
				SNew(SImage).Image(&GWhiteBrush).ColorAndOpacity(Color)
			];
	}
}

// ---------------------------------------------------------------------------------------------
// SEvoswarmSparkline — population-over-time curve for one species, on a shared vertical scale.
// ---------------------------------------------------------------------------------------------
class SEvoswarmSparkline : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoswarmSparkline) : _SpeciesIndex(0), _LineColor(FLinearColor::White) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
		SLATE_ARGUMENT(int32, SpeciesIndex)
		SLATE_ARGUMENT(FLinearColor, LineColor)
		SLATE_ARGUMENT(TSharedPtr<int32>, SharedMax)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Sim = InArgs._Sim;
		SpeciesIndex = InArgs._SpeciesIndex;
		LineColor = InArgs._LineColor;
		SharedMax = InArgs._SharedMax;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(120.f, 20.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());

		// Track background.
		FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(), &GWhiteBrush, ESlateDrawEffect::None, CBarBg);

		const UEvoswarmSimSubsystem* S = Sim.Get();
		if (S && S->GetSpeciesStats().IsValidIndex(SpeciesIndex))
		{
			const FSpeciesLiveStats& St = S->GetSpeciesStats()[SpeciesIndex];
			const int32 N = St.PopHistory.Num();
			const int32 Max = FMath::Max(1, (SharedMax.IsValid() ? *SharedMax : 1));
			if (N >= 2)
			{
				TArray<FVector2D> Points;
				Points.Reserve(N);
				for (int32 I = 0; I < N; ++I)
				{
					const float X = Size.X * (static_cast<float>(I) / (N - 1));
					const float Frac = FMath::Clamp(static_cast<float>(St.PopHistory[I]) / Max, 0.f, 1.f);
					const float Y = Size.Y - Size.Y * Frac;
					Points.Add(FVector2D(X, Y));
				}
				FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(), Points,
					ESlateDrawEffect::None, LineColor, /*bAntialias*/ true, /*Thickness*/ 1.5f);
			}
		}
		return LayerId + 1;
	}

private:
	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
	int32 SpeciesIndex = 0;
	FLinearColor LineColor = FLinearColor::White;
	TSharedPtr<int32> SharedMax;
};

// ---------------------------------------------------------------------------------------------
// SEvoswarmGradientStrip — the diet colour scale, herbivore (green) -> omnivore -> carnivore (red).
// Used in the legend; the same 3-stop gradient underlies the per-species diet bar.
// ---------------------------------------------------------------------------------------------
class SEvoswarmGradientStrip : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoswarmGradientStrip) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&) {}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(120.f, 10.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		TArray<FSlateGradientStop> Stops;
		Stops.Add(FSlateGradientStop(FVector2f(0.f, 0.f), Evo::DietColor(0.f)));
		Stops.Add(FSlateGradientStop(FVector2f((float)Size.X * 0.5f, 0.f), Evo::DietColor(0.5f)));
		Stops.Add(FSlateGradientStop(FVector2f((float)Size.X, 0.f), Evo::DietColor(1.f)));
		FSlateDrawElement::MakeGradient(Out, LayerId, Geometry.ToPaintGeometry(), MoveTemp(Stops),
			Orient_Horizontal, ESlateDrawEffect::None);
		return LayerId;
	}
};

// ---------------------------------------------------------------------------------------------
// SEvoswarmDietBar — the diet scale with the population HISTOGRAM overlaid and the average marker.
// The gradient is the axis; the bright bars show where individuals actually sit on it. Two humps
// mean the species is splitting into diverging diets (speciation the average alone would hide).
// ---------------------------------------------------------------------------------------------
class SEvoswarmDietBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoswarmDietBar) : _SpeciesIndex(0) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
		SLATE_ARGUMENT(int32, SpeciesIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Sim = InArgs._Sim;
		SpeciesIndex = InArgs._SpeciesIndex;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(120.f, 12.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());

		// Diet gradient (the scale).
		{
			TArray<FSlateGradientStop> Stops;
			Stops.Add(FSlateGradientStop(FVector2f(0.f, 0.f), Evo::DietColor(0.f)));
			Stops.Add(FSlateGradientStop(FVector2f((float)Size.X * 0.5f, 0.f), Evo::DietColor(0.5f)));
			Stops.Add(FSlateGradientStop(FVector2f((float)Size.X, 0.f), Evo::DietColor(1.f)));
			FSlateDrawElement::MakeGradient(Out, LayerId, Geometry.ToPaintGeometry(), MoveTemp(Stops),
				Orient_Horizontal, ESlateDrawEffect::None);
		}

		const UEvoswarmSimSubsystem* S = Sim.Get();
		if (S && S->GetSpeciesStats().IsValidIndex(SpeciesIndex))
		{
			const FSpeciesLiveStats& St = S->GetSpeciesStats()[SpeciesIndex];
			const int32 NumBins = St.DietHistogram.Num();
			if (NumBins > 0 && St.Count > 0)
			{
				int32 MaxBin = 1;
				for (int32 Bin : St.DietHistogram)
				{
					MaxBin = FMath::Max(MaxBin, Bin);
				}
				const float BinW = Size.X / NumBins;
				const FLinearColor BarColor(0.97f, 0.98f, 1.f, 0.85f);
				for (int32 B = 0; B < NumBins; ++B)
				{
					if (St.DietHistogram[B] <= 0)
					{
						continue;
					}
					const float Frac = static_cast<float>(St.DietHistogram[B]) / MaxBin;
					const float H = FMath::Max(1.5f, Size.Y * Frac);
					const float X = BinW * B + 1.f;
					FSlateDrawElement::MakeBox(Out, LayerId + 1,
						Geometry.ToPaintGeometry(FVector2f(FMath::Max(1.f, BinW - 2.f), H),
							FSlateLayoutTransform(FVector2f(X, (float)Size.Y - H))),
						&GWhiteBrush, ESlateDrawEffect::None, BarColor);
				}
			}

			// Population-average marker (a white tick spanning the bar).
			const float MarkerX = (float)Size.X * FMath::Clamp(St.AvgDiet, 0.f, 1.f);
			FSlateDrawElement::MakeBox(Out, LayerId + 2,
				Geometry.ToPaintGeometry(FVector2f(2.f, (float)Size.Y + 4.f), FSlateLayoutTransform(FVector2f(MarkerX - 1.f, -2.f))),
				&GWhiteBrush, ESlateDrawEffect::None, CWhite);
		}
		return LayerId + 2;
	}

private:
	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
	int32 SpeciesIndex = 0;
};

// ---------------------------------------------------------------------------------------------
// SEvoswarmSpeciesRow — one species: header (swatch/name/gen/count), sparkline, diet bar,
// averaged genome, and demographics. Text is cached and refreshed on a ~10 Hz throttle.
// ---------------------------------------------------------------------------------------------
class SEvoswarmSpeciesRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoswarmSpeciesRow) : _SpeciesIndex(0) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
		SLATE_ARGUMENT(int32, SpeciesIndex)
		SLATE_ARGUMENT(TSharedPtr<int32>, SharedMax)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Sim = InArgs._Sim;
		SpeciesIndex = InArgs._SpeciesIndex;

		FLinearColor SpeciesColor = FLinearColor::White;
		if (const UEvoswarmSimSubsystem* S = Sim.Get())
		{
			if (S->GetSpeciesStats().IsValidIndex(SpeciesIndex))
			{
				SpeciesColor = S->GetSpeciesStats()[SpeciesIndex].Color;
			}
		}

		Refresh(); // seed cached strings so the row isn't blank on frame 1

		ChildSlot
			[
				SNew(SVerticalBox)

					// --- Header: colour swatch, name, gen, count ---
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
							[
								SNew(SBox).WidthOverride(12.f).HeightOverride(12.f)
									[
										SNew(SImage).Image(&GWhiteBrush).ColorAndOpacity(SpeciesColor)
									]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(FontName()).ColorAndOpacity(FSlateColor(SpeciesColor))
									.Text_Lambda([this] { return NameText; })
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
							[
								SNew(STextBlock).Font(FontSmall()).ColorAndOpacity(FSlateColor(CGrey))
									.Text_Lambda([this] { return GenText; })
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(FontName()).ColorAndOpacity(FSlateColor(CWhite))
									.Text_Lambda([this] { return CountText; })
							]
					]

				// --- Population sparkline ---
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						SNew(SBox).HeightOverride(20.f)
							[
								SNew(SEvoswarmSparkline)
									.Sim(Sim).SpeciesIndex(SpeciesIndex).LineColor(SpeciesColor).SharedMax(InArgs._SharedMax)
							]
					]

				// --- Diet gradient + histogram + average marker ---
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 2.f)
					[
						SNew(SBox).HeightOverride(12.f)
							[
								SNew(SEvoswarmDietBar).Sim(Sim).SpeciesIndex(SpeciesIndex)
							]
					]

					// --- Averaged genome (two lines) ---
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock).Font(FontBody()).ColorAndOpacity(FSlateColor(CGrey))
							.Text_Lambda([this] { return GenomeLine1; })
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(FontBody()).ColorAndOpacity(FSlateColor(CGrey))
							.Text_Lambda([this] { return GenomeLine2; })
					]

					// --- Demographics: births/deaths + rates + kills ---
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(STextBlock).Font(FontBody()).ColorAndOpacity(FSlateColor(CGood))
									.Text_Lambda([this] { return BirthsText; })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
							[
								SNew(STextBlock).Font(FontBody()).ColorAndOpacity(FSlateColor(CBad))
									.Text_Lambda([this] { return DeathsText; })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
							[
								SNew(STextBlock).Font(FontBody()).ColorAndOpacity(FSlateColor(CWhite))
									.Text_Lambda([this] { return KillsText; })
							]
					]
				// --- Death-cause breakdown ---
				+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(FontSmall()).ColorAndOpacity(FSlateColor(CGrey))
							.Text_Lambda([this] { return DeathCausesText; })
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
					[
						MakeRule(FLinearColor(1.f, 1.f, 1.f, 0.06f))
					]
			];
	}

	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
	{
		SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
		RefreshTimer += DeltaTime;
		if (RefreshTimer >= 0.1f) // ~10 Hz
		{
			RefreshTimer = 0.f;
			Refresh();
		}
	}

private:
	void Refresh()
	{
		const UEvoswarmSimSubsystem* S = Sim.Get();
		if (!S || !S->GetSpeciesStats().IsValidIndex(SpeciesIndex))
		{
			return;
		}
		const FSpeciesLiveStats& St = S->GetSpeciesStats()[SpeciesIndex];

		NameText = FText::FromString(St.Name);
		GenText = FText::FromString(FString::Printf(TEXT("gen %.0f / %d"), St.AvgGeneration, St.MaxGeneration));
		CountText = FText::FromString(FString::Printf(TEXT("x %d"), St.Count));

		const TCHAR* DietWord = (St.AvgDiet < Evo::DietHerbThreshold) ? TEXT("herbivore")
			: (St.AvgDiet > Evo::DietCarnThreshold ? TEXT("carnivore") : TEXT("omnivore"));
		GenomeLine1 = FText::FromString(FString::Printf(TEXT("HP %.0f   Spd %.0f   Per %.0f   Life %.0f"),
			St.AvgHP, St.AvgWalkSpeed, St.AvgPerception, St.AvgLifespan));
		GenomeLine2 = FText::FromString(FString::Printf(TEXT("Dmg %.0f   Arm %.0f   Mut %.2f   Diet %.2f (%s)"),
			St.AvgDamage, St.AvgArmor, St.AvgMutationRate, St.AvgDiet, DietWord));

		const float BirthsPerMin = Evo::RatePerMinute(St.BirthHistory);
		const float DeathsPerMin = Evo::RatePerMinute(St.DeathHistory);
		BirthsText = FText::FromString(FString::Printf(TEXT("Born %d (+%.1f/min)"), St.TotalBirths, BirthsPerMin));
		DeathsText = FText::FromString(FString::Printf(TEXT("Died %d (-%.1f/min)"), St.TotalDeaths(), DeathsPerMin));
		KillsText = FText::FromString(FString::Printf(TEXT("Kills %d"), St.TotalKillsMade));
		DeathCausesText = FText::FromString(FString::Printf(
			TEXT("starved %d  \x2022  eaten %d  \x2022  wounds %d  \x2022  old age %d"),
			St.DeathCount(EDeathCause::Starvation), St.DeathCount(EDeathCause::Predation),
			St.DeathCount(EDeathCause::Injury), St.DeathCount(EDeathCause::OldAge)));
	}

	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
	int32 SpeciesIndex = 0;
	float RefreshTimer = 0.f;

	FText NameText, GenText, CountText;
	FText GenomeLine1, GenomeLine2;
	FText BirthsText, DeathsText, KillsText, DeathCausesText;
};

// ---------------------------------------------------------------------------------------------
// SEvoswarmHUD (root)
// ---------------------------------------------------------------------------------------------
void SEvoswarmHUD::Construct(const FArguments& InArgs)
{
	Sim = InArgs._Sim;
	SharedPopMax = MakeShared<int32>(1);

	RefreshGlobalBar(0.f);

	// Legend row: colour swatch scale (diet) + the shape/brightness cheat-sheet.
	TSharedRef<SWidget> Legend =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Font(FontSmall()).ColorAndOpacity(FSlateColor(CGrey))
				.Text(LOCTEXT("LegendDiet", "DIET  herbivore \x2192 omnivore \x2192 carnivore"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(SBox).HeightOverride(8.f)[SNew(SEvoswarmGradientStrip)]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
		[
			SNew(STextBlock).Font(FontSmall()).ColorAndOpacity(FSlateColor(CGrey))
				.Text(LOCTEXT("LegendColor", "brightness = vigour (HP + armour + damage)"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Font(FontSmall()).ColorAndOpacity(FSlateColor(CGrey))
				.Text(LOCTEXT("LegendShape", "silhouette  bulk: HP \x00b7 length: run speed \x00b7 width: armour"))
		];

	ChildSlot
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(24.f)
		[
			SNew(SBox).WidthOverride(PanelWidth)
				[
					SNew(SBorder)
						.BorderImage(&GWhiteBrush)
						.BorderBackgroundColor(CPanelBg)
						.Padding(FMargin(14.f, 12.f))
						[
							SNew(SVerticalBox)

								// Top accent strip.
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SBox).HeightOverride(3.f)[SNew(SImage).Image(&GWhiteBrush).ColorAndOpacity(CAccent)]
								]

								// --- Global bar ---
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
								[
									SNew(STextBlock).Font(FontTitle()).ColorAndOpacity(FSlateColor(CWhite))
										.Text(LOCTEXT("Title", "EVOSWARM"))
								]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
										[
											SNew(STextBlock).Font(FontBody()).ColorAndOpacity(FSlateColor(CGrey))
												.Text_Lambda([this] { return SummaryText; })
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(STextBlock).Font(FontBody())
												.ColorAndOpacity_Lambda([this] { return FpsColor; })
												.Text_Lambda([this] { return FpsText; })
										]
								]

							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
								[
									MakeRule(FLinearColor(1.f, 1.f, 1.f, 0.08f))
								]

								// --- Legend ---
								+ SVerticalBox::Slot().AutoHeight()[Legend]

								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
								[
									MakeRule(FLinearColor(1.f, 1.f, 1.f, 0.08f))
								]

								// --- Per-species rows (filled lazily once the sim reports species) ---
								+ SVerticalBox::Slot().AutoHeight()
								[
									SAssignNew(SpeciesContainer, SVerticalBox)
								]

								// --- Event feed header ---
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 4.f)
								[
									SNew(STextBlock).Font(FontSmall()).ColorAndOpacity(FSlateColor(CGrey))
										.Text(LOCTEXT("Events", "EVENTS"))
								]
								// --- Scrollable event feed ---
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SBox).HeightOverride(120.f)
										[
											SAssignNew(EventScroll, SScrollBox)
										]
								]
						]
				]
		];
}

void SEvoswarmHUD::BuildSpeciesRows()
{
	const UEvoswarmSimSubsystem* S = Sim.Get();
	if (!S || !SpeciesContainer.IsValid())
	{
		return;
	}

	SpeciesContainer->ClearChildren();
	const int32 Num = S->GetSpeciesStats().Num();
	for (int32 I = 0; I < Num; ++I)
	{
		SpeciesContainer->AddSlot().AutoHeight().Padding(0.f, 4.f)
			[
				SNew(SEvoswarmSpeciesRow).Sim(Sim).SpeciesIndex(I).SharedMax(SharedPopMax)
			];
	}
	bRowsBuilt = true;
}

void SEvoswarmHUD::RefreshGlobalBar(float DeltaTime)
{
	// Smoothed FPS (exponential moving average) reads more steadily than a raw per-frame value.
	if (DeltaTime > 0.f)
	{
		const float Instant = 1.f / DeltaTime;
		SmoothedFps = FMath::Lerp(SmoothedFps, Instant, 0.1f);
	}

	const UEvoswarmSimSubsystem* S = Sim.Get();
	int32 TotalPop = 0;
	int32 PeakGen = 0;
	int32 FoodCount = 0;
	float Elapsed = 0.f;
	int32 NewMax = 1;
	if (S)
	{
		for (const FSpeciesLiveStats& St : S->GetSpeciesStats())
		{
			TotalPop += St.Count;
			PeakGen = FMath::Max(PeakGen, St.MaxGeneration);
			for (int32 V : St.PopHistory)
			{
				NewMax = FMath::Max(NewMax, V);
			}
		}
		FoodCount = S->GetFoodCount();
		Elapsed = S->GetElapsedTime();
	}
	if (SharedPopMax.IsValid())
	{
		*SharedPopMax = NewMax;
	}

	SummaryText = FText::FromString(FString::Printf(TEXT("Boids %d    Food %d    Gen %d    %s"),
		TotalPop, FoodCount, PeakGen, *ClockText(Elapsed).ToString()));

	FpsText = FText::FromString(FString::Printf(TEXT("%.0f FPS"), SmoothedFps));
	FpsColor = FSlateColor((SmoothedFps >= 50.f) ? FLinearColor(0.45f, 0.9f, 0.5f, 1.f)
		: (SmoothedFps >= 30.f) ? FLinearColor(0.95f, 0.8f, 0.35f, 1.f)
		: FLinearColor(0.95f, 0.4f, 0.4f, 1.f));
}

void SEvoswarmHUD::RefreshEventFeed()
{
	const UEvoswarmSimSubsystem* S = Sim.Get();
	if (!S || !EventScroll.IsValid())
	{
		return;
	}

	const TArray<FSimEvent>& Events = S->GetEventLog();
	// Only rebuild when the count changes, so the feed stays freely scrollable between events.
	if (Events.Num() == LastEventCount)
	{
		return;
	}
	LastEventCount = Events.Num();

	EventScroll->ClearChildren();
	for (const FSimEvent& E : Events) // oldest first -> newest lands at the bottom
	{
		EventScroll->AddSlot().Padding(0.f, 1.f)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock).Font(FontSmall()).ColorAndOpacity(FSlateColor(CGrey))
							.Text(ClockText(E.Time))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock).Font(FontSmall()).ColorAndOpacity(FSlateColor(E.Color))
							.AutoWrapText(true)
							.Text(FText::FromString(E.Text))
					]
			];
	}
	EventScroll->ScrollToEnd(); // reveal the newest event
}

void SEvoswarmHUD::Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);

	// Build the species rows once the sim is up and reporting species.
	if (!bRowsBuilt)
	{
		if (const UEvoswarmSimSubsystem* S = Sim.Get())
		{
			if (S->GetSpeciesStats().Num() > 0)
			{
				BuildSpeciesRows();
			}
		}
	}

	RefreshTimer += DeltaTime;
	if (RefreshTimer >= 0.1f) // ~10 Hz for text + event feed
	{
		RefreshGlobalBar(RefreshTimer);
		RefreshEventFeed();
		RefreshTimer = 0.f;
	}
}

#undef LOCTEXT_NAMESPACE