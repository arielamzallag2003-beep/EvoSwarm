// Copyright Evoswarm.

#include "SEvoswarmHUD.h"
#include "EvoswarmSimSubsystem.h"
#include "EvoswarmTuning.h"
#include "SEvoswarmAnalytics.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SLeafWidget.h"

#include "EvoswarmHUDStyle.h"
#include "Rendering/DrawElements.h"

#define LOCTEXT_NAMESPACE "Evoswarm"

// ---------------------------------------------------------------------------------------------
// SEvoswarmSparkline
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
		FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(), &EvoHud::WhiteBrush, ESlateDrawEffect::None, EvoHud::BarBg);

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
					const float X = (float)Size.X * (static_cast<float>(I) / (N - 1));
					const float Frac = FMath::Clamp(static_cast<float>(St.PopHistory[I]) / Max, 0.f, 1.f);
					const float Y = (float)Size.Y - (float)Size.Y * Frac;
					Points.Add(FVector2D(X, Y));
				}
				FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(), Points,
					ESlateDrawEffect::None, LineColor, true, 1.5f);
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
// SEvoswarmGradientStrip
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
// SEvoswarmDietBar
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
				const float BinW = (float)Size.X / NumBins;
				const FLinearColor BarColor(0.97f, 0.98f, 1.f, 0.85f);
				for (int32 B = 0; B < NumBins; ++B)
				{
					if (St.DietHistogram[B] <= 0) { continue; }
					const float Frac = static_cast<float>(St.DietHistogram[B]) / MaxBin;
					const float H = FMath::Max(1.5f, (float)Size.Y * Frac);
					const float X = BinW * B + 1.f;
					FSlateDrawElement::MakeBox(Out, LayerId + 1,
						Geometry.ToPaintGeometry(FVector2f(FMath::Max(1.f, BinW - 2.f), H),
							FSlateLayoutTransform(FVector2f(X, (float)Size.Y - H))),
						&EvoHud::WhiteBrush, ESlateDrawEffect::None, BarColor);
				}
			}

			const float MarkerX = (float)Size.X * FMath::Clamp(St.AvgDiet, 0.f, 1.f);
			FSlateDrawElement::MakeBox(Out, LayerId + 2,
				Geometry.ToPaintGeometry(FVector2f(2.f, (float)Size.Y + 4.f), FSlateLayoutTransform(FVector2f(MarkerX - 1.f, -2.f))),
				&EvoHud::WhiteBrush, ESlateDrawEffect::None, EvoHud::White);
		}
		return LayerId + 2;
	}

private:
	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
	int32 SpeciesIndex = 0;
};

// ---------------------------------------------------------------------------------------------
// SEvoswarmStatBar painted HP / stamina / hunger bar for the inspector panel.
// ---------------------------------------------------------------------------------------------
class SEvoswarmStatBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoswarmStatBar) {}
		SLATE_ATTRIBUTE(float, Fraction)
		SLATE_ATTRIBUTE(FLinearColor, BarColor)
		SLATE_ATTRIBUTE(FString, Label)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FractionAttr = InArgs._Fraction;
		BarColorAttr = InArgs._BarColor;
		LabelAttr = InArgs._Label;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(200.f, 14.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());

		// Track background.
		FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(), &EvoHud::WhiteBrush, ESlateDrawEffect::None, EvoHud::BarBg);

		// Filled portion.
		const float F = FMath::Clamp(FractionAttr.Get(), 0.f, 1.f);
		if (F > 0.01f)
		{
			FSlateDrawElement::MakeBox(Out, LayerId + 1,
				Geometry.ToPaintGeometry(FVector2f((float)Size.X * F, (float)Size.Y), FSlateLayoutTransform(FVector2f(0.f, 0.f))),
				&EvoHud::WhiteBrush, ESlateDrawEffect::None, BarColorAttr.Get());
		}

		// Label text overlay.
		const FString Lbl = LabelAttr.Get();
		if (!Lbl.IsEmpty())
		{
			const FSlateFontInfo Font = EvoHud::FontSmall();
			FSlateDrawElement::MakeText(Out, LayerId + 2,
				Geometry.ToPaintGeometry(FVector2f((float)Size.X - 6.f, (float)Size.Y),
					FSlateLayoutTransform(FVector2f(4.f, 0.f))),
				Lbl, Font, ESlateDrawEffect::None, EvoHud::White);
		}

		return LayerId + 2;
	}

private:
	TAttribute<float> FractionAttr;
	TAttribute<FLinearColor> BarColorAttr;
	TAttribute<FString> LabelAttr;
};

// ---------------------------------------------------------------------------------------------
// SEvoswarmInspector right-side panel showing the inspected creature's live data.
// ---------------------------------------------------------------------------------------------
class SEvoswarmInspector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoswarmInspector) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Sim = InArgs._Sim;

		// Slate does not tick collapsed widgets, so a panel that collapses itself can never
		// un-collapse from its own Tick. Keep the ROOT alive (and click-through) and drive the
		// CONTENT's visibility from a bound attribute instead: the parent evaluates that during
		// arrangement every frame, whether or not this widget is currently ticking.
		SetVisibility(EVisibility::SelfHitTestInvisible);

		ChildSlot
			[
				SNew(SBox).WidthOverride(EvoHud::InspectWidth)
					.Visibility(this, &SEvoswarmInspector::ContentVisibility)
					[
						SNew(SBorder)
							.BorderImage(&EvoHud::WhiteBrush)
							.BorderBackgroundColor(EvoHud::PanelBg)
							.Padding(FMargin(12.f, 10.f))
							[
								SNew(SVerticalBox)

									// Accent strip.
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(SBox).HeightOverride(3.f)
											[
												SAssignNew(AccentImage, SImage).Image(&EvoHud::WhiteBrush).ColorAndOpacity(EvoHud::Accent)
											]
									]

									// Header: swatch + species name.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
									[
										SNew(SHorizontalBox)
											+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
											[
												SNew(SBox).WidthOverride(12.f).HeightOverride(12.f)
													[
														SAssignNew(SwatchImage, SImage).Image(&EvoHud::WhiteBrush).ColorAndOpacity(EvoHud::White)
													]
											]
											+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
											[
												SNew(STextBlock).Font(EvoHud::FontName()).ColorAndOpacity(FSlateColor(EvoHud::White))
													.Text_Lambda([this] { return SpeciesText; })
											]
									]

								// Generation + age / lifespan.
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
									[
										SNew(STextBlock).Font(EvoHud::FontBody()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
											.Text_Lambda([this] { return GenAgeText; })
									]

									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)[EvoHud::MakeRule(FLinearColor(1, 1, 1, 0.08f))]

									// HP bar.
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
									[
										SNew(SBox).HeightOverride(14.f)
											[
												SNew(SEvoswarmStatBar)
													.Fraction_Lambda([this] { return HPFrac; })
													.BarColor_Lambda([this] { return FMath::Lerp(FLinearColor(0.85f, 0.2f, 0.2f), FLinearColor(0.3f, 0.85f, 0.35f), HPFrac); })
													.Label_Lambda([this] { return HPLabel; })
											]
									]
								// Stamina bar.
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
									[
										SNew(SBox).HeightOverride(14.f)
											[
												SNew(SEvoswarmStatBar)
													.Fraction_Lambda([this] { return StamFrac; })
													.BarColor(FLinearColor(0.85f, 0.75f, 0.25f))
													.Label_Lambda([this] { return StamLabel; })
											]
									]
								// Hunger bar.
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
									[
										SNew(SBox).HeightOverride(14.f)
											[
												SNew(SEvoswarmStatBar)
													.Fraction_Lambda([this] { return HungerFrac; })
													.BarColor(FLinearColor(0.90f, 0.55f, 0.18f))
													.Label_Lambda([this] { return HungerLabel; })
											]
									]

								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)[EvoHud::MakeRule(FLinearColor(1, 1, 1, 0.08f))]

									// Full genome (compact, 4 lines).
									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
									[
										SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
											.Text_Lambda([this] { return GenomeLine1; })
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
											.Text_Lambda([this] { return GenomeLine2; })
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
											.Text_Lambda([this] { return GenomeLine3; })
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
											.Text_Lambda([this] { return GenomeLine4; })
									]

									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)[EvoHud::MakeRule(FLinearColor(1, 1, 1, 0.08f))]

									// Live state (adrenaline, cooldowns, repro count).
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
											.Text_Lambda([this] { return StateLine1; })
									]
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
											.Text_Lambda([this] { return StateLine2; })
									]

									+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 2.f)[EvoHud::MakeRule(FLinearColor(1, 1, 1, 0.08f))]

									// Status / instruction line.
									+ SVerticalBox::Slot().AutoHeight()
									[
										SNew(STextBlock).Font(EvoHud::FontBody())
											.ColorAndOpacity_Lambda([this] { return StatusColor; })
											.Text_Lambda([this] { return StatusText; })
									]
							]
					]
			];
	}

	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
	{
		SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);

		const UEvoswarmSimSubsystem* S = Sim.Get();
		if (!S)
		{
			return;
		}

		const FBoidInspectState& Ins = S->GetInspect();
		if (!Ins.bActive)
		{
			return;
		}

		// ~10 Hz text refresh, but a newly picked creature must not wait for the next slot,
		// or the panel pops up showing the previous subject's numbers for up to 100 ms.
		const bool bNewSubject = (Ins.Entity != LastEntity) || (Ins.bDeceased != bLastDeceased);
		RefreshTimer += DeltaTime;
		if (RefreshTimer < 0.1f && !bNewSubject)
		{
			return;
		}
		RefreshTimer = 0.f;
		LastEntity = Ins.Entity;
		bLastDeceased = Ins.bDeceased;

		// --- Species identity ---
		FString Name = TEXT("???");
		FLinearColor Color = EvoHud::White;
		if (S->GetSpeciesStats().IsValidIndex(Ins.SpeciesIndex))
		{
			const FSpeciesLiveStats& Sp = S->GetSpeciesStats()[Ins.SpeciesIndex];
			Name = Sp.Name;
			Color = Sp.Color;
		}

		if (Ins.bDeceased)
		{
			SpeciesText = FText::FromString(FString::Printf(TEXT("\x2620 %s  DECEASED"), *Name));
			AccentImage->SetColorAndOpacity(FLinearColor(0.8f, 0.2f, 0.2f));
		}
		else
		{
			SpeciesText = FText::FromString(Name);
			AccentImage->SetColorAndOpacity(Color);
		}
		SwatchImage->SetColorAndOpacity(Color);

		GenAgeText = FText::FromString(FString::Printf(TEXT("Gen %d    Age %.1fs / %.0fs"),
			Ins.Generation, Ins.Age, Ins.Lifespan));

		// --- Vital bars ---
		HPFrac = (Ins.MaxHP > 0.f) ? FMath::Clamp(Ins.HP / Ins.MaxHP, 0.f, 1.f) : 0.f;
		StamFrac = (Ins.MaxStam > 0.f) ? FMath::Clamp(Ins.Stam / Ins.MaxStam, 0.f, 1.f) : 0.f;
		HungerFrac = (Ins.MaxHunger > 0.f) ? FMath::Clamp(Ins.Hunger / Ins.MaxHunger, 0.f, 1.f) : 0.f;

		HPLabel = FString::Printf(TEXT("HP  %.0f / %.0f"), Ins.HP, Ins.MaxHP);
		StamLabel = FString::Printf(TEXT("STA  %.0f / %.0f"), Ins.Stam, Ins.MaxStam);
		HungerLabel = FString::Printf(TEXT("HUN  %.0f / %.0f"), Ins.Hunger, Ins.MaxHunger);

		// --- Full genome ---
		const FBoidGenome& G = Ins.Genome;
		GenomeLine1 = FText::FromString(FString::Printf(TEXT("HP %.1f  Arm %.1f  Dmg %.1f  Intim %.1f"),
			G.Get(EBoidStat::HP), G.Get(EBoidStat::Armor), G.Get(EBoidStat::Damage), G.Get(EBoidStat::Intimidation)));
		GenomeLine2 = FText::FromString(FString::Printf(TEXT("Walk %.1f  Run %.1f  Sta %.1f  Aggr %.1f"),
			G.Get(EBoidStat::WalkSpeed), G.Get(EBoidStat::RunSpeed), G.Get(EBoidStat::Stamina), G.Get(EBoidStat::Aggressiveness)));
		GenomeLine3 = FText::FromString(FString::Printf(TEXT("Per %.1f  Stl %.1f  Hun %.1f  Bio %.1f"),
			G.Get(EBoidStat::Perception), G.Get(EBoidStat::Stealth), G.Get(EBoidStat::Hunger), G.Get(EBoidStat::Biomass)));

		const TCHAR* DietWord = (G.Get(EBoidStat::Diet) < Evo::DietHerbThreshold) ? TEXT("herb")
			: (G.Get(EBoidStat::Diet) > Evo::DietCarnThreshold ? TEXT("carn") : TEXT("omni"));
		GenomeLine4 = FText::FromString(FString::Printf(TEXT("Diet %.2f (%s)  Life %.1f  Repr %.1f  Mut %.2f  Int %.1f  Reg %.1f"),
			G.Get(EBoidStat::Diet), DietWord, G.Get(EBoidStat::Lifespan), G.Get(EBoidStat::ReproductionRate),
			G.Get(EBoidStat::MutationRate), G.Get(EBoidStat::Integration), G.Get(EBoidStat::Regeneration)));

		// --- Live state ---
		StateLine1 = FText::FromString(FString::Printf(TEXT("Repro x%d    Adrenaline %.1fs"),
			Ins.ReproCount, Ins.Adrenaline));
		StateLine2 = FText::FromString(FString::Printf(TEXT("Cooldowns:  atk %.1f    repro %.1f"),
			Ins.AttackCooldown, Ins.ReproCooldown));

		// --- Status ---
		if (Ins.bDeceased)
		{
			StatusText = LOCTEXT("StatusDead", "DECEASED  \x2014  F to dismiss");
			StatusColor = FSlateColor(FLinearColor(0.85f, 0.35f, 0.35f));
		}
		else if (Ins.bLocked)
		{
			StatusText = LOCTEXT("StatusLocked", "\x25C9 LOCKED  \x2014  F to unlock");
			StatusColor = FSlateColor(FLinearColor(1.f, 0.85f, 0.25f));
		}
		else
		{
			StatusText = LOCTEXT("StatusHover", "F to lock on this creature");
			StatusColor = FSlateColor(EvoHud::Grey);
		}
	}

private:
	/* Bound attribute: evaluated by the parent during arrangement, so it works while collapsed. */
	EVisibility ContentVisibility() const
	{
		const UEvoswarmSimSubsystem* S = Sim.Get();
		return (S && S->GetInspect().bActive) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
	}

	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
	float RefreshTimer = 0.f;

	// Change tracking, so switching subject refreshes the text on the very next tick.
	FMassEntityHandle LastEntity;
	bool bLastDeceased = false;

	TSharedPtr<SImage> AccentImage;
	TSharedPtr<SImage> SwatchImage;

	FText SpeciesText, GenAgeText;
	float HPFrac = 0.f, StamFrac = 0.f, HungerFrac = 0.f;
	FString HPLabel, StamLabel, HungerLabel;
	FText GenomeLine1, GenomeLine2, GenomeLine3, GenomeLine4;
	FText StateLine1, StateLine2;
	FText StatusText;
	FSlateColor StatusColor = FSlateColor(EvoHud::Grey);
};

// ---------------------------------------------------------------------------------------------
// SEvoswarmSpeciesRow
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

		Refresh();

		ChildSlot
			[
				SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 6.f, 0.f)
							[
								SNew(SBox).WidthOverride(12.f).HeightOverride(12.f)
									[SNew(SImage).Image(&EvoHud::WhiteBrush).ColorAndOpacity(SpeciesColor)]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(EvoHud::FontName()).ColorAndOpacity(FSlateColor(SpeciesColor))
									.Text_Lambda([this] { return NameText; })
							]
							+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f, 0.f, 0.f, 0.f)
							[
								SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
									.Text_Lambda([this] { return GenText; })
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock).Font(EvoHud::FontName()).ColorAndOpacity(FSlateColor(EvoHud::White))
									.Text_Lambda([this] { return CountText; })
							]
					]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
					[
						SNew(SBox).HeightOverride(20.f)
							[
								SNew(SEvoswarmSparkline)
									.Sim(Sim).SpeciesIndex(SpeciesIndex).LineColor(SpeciesColor).SharedMax(InArgs._SharedMax)
							]
					]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 2.f)
					[
						SNew(SBox).HeightOverride(12.f)
							[SNew(SEvoswarmDietBar).Sim(Sim).SpeciesIndex(SpeciesIndex)]
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[
						SNew(STextBlock).Font(EvoHud::FontBody()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
							.Text_Lambda([this] { return GenomeLine1; })
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(EvoHud::FontBody()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
							.Text_Lambda([this] { return GenomeLine2; })
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
					[
						SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(STextBlock).Font(EvoHud::FontBody()).ColorAndOpacity(FSlateColor(EvoHud::Good))
									.Text_Lambda([this] { return BirthsText; })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
							[
								SNew(STextBlock).Font(EvoHud::FontBody()).ColorAndOpacity(FSlateColor(EvoHud::Bad))
									.Text_Lambda([this] { return DeathsText; })
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(14.f, 0.f, 0.f, 0.f)
							[
								SNew(STextBlock).Font(EvoHud::FontBody()).ColorAndOpacity(FSlateColor(EvoHud::White))
									.Text_Lambda([this] { return KillsText; })
							]
					]
				+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
							.Text_Lambda([this] { return DeathCausesText; })
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
					[EvoHud::MakeRule(FLinearColor(1.f, 1.f, 1.f, 0.06f))]
			];
	}

	virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
	{
		SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
		RefreshTimer += DeltaTime;
		if (RefreshTimer >= 0.1f)
		{
			RefreshTimer = 0.f;
			Refresh();
		}
	}

private:
	void Refresh()
	{
		const UEvoswarmSimSubsystem* S = Sim.Get();
		if (!S || !S->GetSpeciesStats().IsValidIndex(SpeciesIndex)) { return; }
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
// SEvoswarmHUD (root) full-viewport SOverlay: left panel + right inspector + crosshair dot.
// ---------------------------------------------------------------------------------------------
void SEvoswarmHUD::Construct(const FArguments& InArgs)
{
	Sim = InArgs._Sim;
	SharedPopMax = MakeShared<int32>(1);

	RefreshGlobalBar(0.f);

	// Legend.
	TSharedRef<SWidget> Legend =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
				.Text(LOCTEXT("LegendDiet", "DIET  herbivore \x2192 omnivore \x2192 carnivore"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[SNew(SBox).HeightOverride(8.f)[SNew(SEvoswarmGradientStrip)]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
		[
			SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
				.Text(LOCTEXT("LegendColor", "brightness = vigour (HP + armour + damage)"))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
				.Text(LOCTEXT("LegendShape", "silhouette  bulk: HP \x00b7 length: run speed \x00b7 width: armour"))
		];

	// Left ecosystem panel.
	TSharedRef<SWidget> LeftPanel =
		SNew(SBox).WidthOverride(EvoHud::PanelWidth)
		[
			SNew(SBorder)
				.BorderImage(&EvoHud::WhiteBrush)
				.BorderBackgroundColor(EvoHud::PanelBg)
				.Padding(FMargin(14.f, 12.f))
				[
					SNew(SVerticalBox)

						+ SVerticalBox::Slot().AutoHeight()
						[SNew(SBox).HeightOverride(3.f)[SNew(SImage).Image(&EvoHud::WhiteBrush).ColorAndOpacity(EvoHud::Accent)]]

						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
						[
							SNew(STextBlock).Font(EvoHud::FontTitle()).ColorAndOpacity(FSlateColor(EvoHud::White))
								.Text(LOCTEXT("Title", "EVOSWARM"))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
						[
							SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(EvoHud::FontBody()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
										.Text_Lambda([this] { return SummaryText; })
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
								[
									SNew(STextBlock).Font(EvoHud::FontBody())
										.ColorAndOpacity_Lambda([this] { return FpsColor; })
										.Text_Lambda([this] { return FpsText; })
								]
						]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[EvoHud::MakeRule(FLinearColor(1, 1, 1, 0.08f))]
						+ SVerticalBox::Slot().AutoHeight()[Legend]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)[EvoHud::MakeRule(FLinearColor(1, 1, 1, 0.08f))]

						+ SVerticalBox::Slot().AutoHeight()
						[SAssignNew(SpeciesContainer, SVerticalBox)]

						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 4.f)
						[
							SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
								.Text(LOCTEXT("Events", "EVENTS"))
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SBox).HeightOverride(120.f)
								[SAssignNew(EventScroll, SScrollBox)]
						]

						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)[EvoHud::MakeRule(FLinearColor(1, 1, 1, 0.08f))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
						[
							SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Dim))
								.Text(LOCTEXT("HintAnalytics", "G  analytics dashboard      B  debug overlay      F  inspect"))
						]
				]
		];

	// --- Root: full-viewport overlay ---
	ChildSlot
		[
			SNew(SOverlay)

				// Left panel.
				+ SOverlay::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(24.f)
				[LeftPanel]

				// Right inspector panel.
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(24.f)
				[
					SNew(SEvoswarmInspector).Sim(Sim)
				]

				// Center-screen crosshair dot (tiny, always visible).
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(6.f).HeightOverride(6.f)
						[SNew(SImage).Image(&EvoHud::WhiteBrush).ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.35f))]
				]

				// Analytics dashboard (G). Last slot, so when it opens it covers everything above.
				+ SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SNew(SEvoswarmAnalytics).Sim(Sim)
				]
		];
}

void SEvoswarmHUD::BuildSpeciesRows()
{
	const UEvoswarmSimSubsystem* S = Sim.Get();
	if (!S || !SpeciesContainer.IsValid()) { return; }

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
	if (DeltaTime > 0.f)
	{
		const float Instant = 1.f / DeltaTime;
		SmoothedFps = FMath::Lerp(SmoothedFps, Instant, 0.1f);
	}

	const UEvoswarmSimSubsystem* S = Sim.Get();
	int32 TotalPop = 0, PeakGen = 0, FoodCount = 0;
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
		TotalPop, FoodCount, PeakGen, *EvoHud::ClockText(Elapsed).ToString()));

	FpsText = FText::FromString(FString::Printf(TEXT("%.0f FPS"), SmoothedFps));
	FpsColor = FSlateColor((SmoothedFps >= 50.f) ? FLinearColor(0.45f, 0.9f, 0.5f, 1.f)
		: (SmoothedFps >= 30.f) ? FLinearColor(0.95f, 0.8f, 0.35f, 1.f)
		: FLinearColor(0.95f, 0.4f, 0.4f, 1.f));
}

void SEvoswarmHUD::RefreshEventFeed()
{
	const UEvoswarmSimSubsystem* S = Sim.Get();
	if (!S || !EventScroll.IsValid()) { return; }

	const TArray<FSimEvent>& Events = S->GetEventLog();
	if (Events.Num() == LastEventCount) { return; }
	LastEventCount = Events.Num();

	EventScroll->ClearChildren();
	for (const FSimEvent& E : Events)
	{
		EventScroll->AddSlot().Padding(0.f, 1.f)
			[
				SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 6.f, 0.f)
					[
						SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
							.Text(EvoHud::ClockText(E.Time))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(E.Color))
							.AutoWrapText(true)
							.Text(FText::FromString(E.Text))
					]
			];
	}
	EventScroll->ScrollToEnd();
}

void SEvoswarmHUD::Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);

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
	if (RefreshTimer >= 0.1f)
	{
		RefreshGlobalBar(RefreshTimer);
		RefreshEventFeed();
		RefreshTimer = 0.f;
	}
}

#undef LOCTEXT_NAMESPACE