// Copyright Evoswarm.

#include "SEvoswarmAnalytics.h"
#include "EvoswarmSimSubsystem.h"
#include "EvoswarmAnalytics.h"
#include "EvoswarmHUDStyle.h"
#include "EvoswarmTuning.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SLeafWidget.h"

#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

#define LOCTEXT_NAMESPACE "Evoswarm"

// =============================================================================================
// Painting helpers shared by the four charts
// =============================================================================================
namespace
{
	/** The rectangle a chart plots into, in widget-local pixels. Fy = 0 is the BOTTOM. */
	struct FPlot
	{
		float X0 = 0.f;
		float Y0 = 0.f;
		float W = 1.f;
		float H = 1.f;

		float PxX(float Fx) const { return X0 + Fx * W; }
		float PxY(float Fy) const { return Y0 + H * (1.f - Fy); }
		FVector2D At(float Fx, float Fy) const { return FVector2D(PxX(Fx), PxY(Fy)); }
	};

	void PaintBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
		float X, float Y, float W, float H, const FLinearColor& C)
	{
		if (W <= 0.f || H <= 0.f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(Out, Layer,
			G.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))),
			&EvoHud::WhiteBrush, ESlateDrawEffect::None, C);
	}

	void PaintText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
		float X, float Y, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& C)
	{
		if (Text.IsEmpty())
		{
			return;
		}
		FSlateDrawElement::MakeText(Out, Layer,
			G.ToPaintGeometry(FVector2f(1200.f, 20.f), FSlateLayoutTransform(FVector2f(X, Y))),
			Text, Font, ESlateDrawEffect::None, C);
	}

	float TextWidth(const FString& Text, const FSlateFontInfo& Font)
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		return static_cast<float>(Measure->Measure(Text, Font).X);
	}

	void PaintTextRight(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
		float RightX, float Y, const FString& Text, const FSlateFontInfo& Font, const FLinearColor& C)
	{
		PaintText(Out, Layer, G, RightX - TextWidth(Text, Font), Y, Text, Font, C);
	}

	void PaintLine(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
		const TArray<FVector2D>& Points, const FLinearColor& C, float Thickness)
	{
		if (Points.Num() < 2)
		{
			return;
		}
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points,
			ESlateDrawEffect::None, C, true, Thickness);
	}

	/** Tick spacing rounded to 1 / 2 / 5 x 10^k, so axis labels land on readable numbers. */
	float NiceStep(float Range, int32 TargetTicks)
	{
		if (Range <= KINDA_SMALL_NUMBER || TargetTicks <= 0)
		{
			return 1.f;
		}
		const float Raw = Range / TargetTicks;
		const float Mag = FMath::Pow(10.f, FMath::FloorToFloat(FMath::LogX(10.f, Raw)));
		const float Norm = Raw / Mag;
		const float Step = (Norm < 1.5f) ? 1.f : (Norm < 3.5f) ? 2.f : (Norm < 7.5f) ? 5.f : 10.f;
		return Step * Mag;
	}

	FString FormatCompact(float V)
	{
		if (FMath::Abs(V) >= 10000.f) { return FString::Printf(TEXT("%.0fk"), V / 1000.f); }
		if (FMath::Abs(V) >= 1000.f) { return FString::Printf(TEXT("%.1fk"), V / 1000.f); }
		if (FMath::Abs(V) >= 100.f) { return FString::Printf(TEXT("%.0f"), V); }
		if (FMath::Abs(V) >= 10.f) { return FString::Printf(TEXT("%.1f"), V); }
		return FString::Printf(TEXT("%.2f"), V);
	}

	/** Plot background, value gridlines + left labels, time gridlines + m:ss labels. */
	void PaintFrame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FPlot& P,
		float VMin, float VMax, float TMin, float TMax)
	{
		PaintBox(Out, Layer, G, P.X0, P.Y0, P.W, P.H, FLinearColor(0.05f, 0.06f, 0.08f, 0.9f));

		const float VRange = FMath::Max(VMax - VMin, KINDA_SMALL_NUMBER);
		const float VStep = NiceStep(VRange, 5);
		const float VFirst = FMath::CeilToFloat(VMin / VStep) * VStep;
		for (float V = VFirst; V <= VMax + KINDA_SMALL_NUMBER; V += VStep)
		{
			const float Fy = (V - VMin) / VRange;
			const float Y = P.PxY(Fy);
			PaintBox(Out, Layer + 1, G, P.X0, Y, P.W, 1.f, EvoHud::Grid);
			PaintTextRight(Out, Layer + 2, G, P.X0 - 6.f, Y - 6.f, FormatCompact(V), EvoHud::FontTiny(), EvoHud::Dim);
		}

		const float TRange = FMath::Max(TMax - TMin, KINDA_SMALL_NUMBER);
		const float TStep = NiceStep(TRange, 6);
		const float TFirst = FMath::CeilToFloat(TMin / TStep) * TStep;
		for (float T = TFirst; T <= TMax + KINDA_SMALL_NUMBER; T += TStep)
		{
			const float Fx = (T - TMin) / TRange;
			const float X = P.PxX(Fx);
			PaintBox(Out, Layer + 1, G, X, P.Y0, 1.f, P.H, EvoHud::Grid);
			const FString Label = EvoHud::ClockText(T).ToString();
			PaintText(Out, Layer + 2, G, X - TextWidth(Label, EvoHud::FontTiny()) * 0.5f,
				P.Y0 + P.H + 4.f, Label, EvoHud::FontTiny(), EvoHud::Dim);
		}

		// Axes.
		PaintBox(Out, Layer + 3, G, P.X0, P.Y0 + P.H, P.W, 1.f, EvoHud::GridStrong);
		PaintBox(Out, Layer + 3, G, P.X0, P.Y0, 1.f, P.H, EvoHud::GridStrong);
	}

	/** Nearest recorded sample to a sim time, or null when the timeline cannot cover it. */
	const FSpeciesTimeSample* SampleAtTime(const FSpeciesTimeline& Line, float Time)
	{
		const int32 N = Line.Samples.Num();
		if (N == 0)
		{
			return nullptr;
		}
		if (N == 1 || Line.Interval <= 0.f)
		{
			return &Line.Samples[0];
		}
		// Uniform spacing, so this is a straight index computation - no search needed.
		const int32 Index = FMath::Clamp(
			FMath::RoundToInt((Time - Line.Samples[0].Time) / Line.Interval), 0, N - 1);
		return &Line.Samples[Index];
	}

	/** Time window covered by every species' timeline. False when nothing is recorded yet. */
	bool RunTimeBounds(const TArray<FSpeciesLiveStats>& Stats, float& OutMin, float& OutMax)
	{
		OutMin = TNumericLimits<float>::Max();
		OutMax = TNumericLimits<float>::Lowest();
		bool bAny = false;
		for (const FSpeciesLiveStats& S : Stats)
		{
			if (S.Timeline.Num() < 2)
			{
				continue;
			}
			OutMin = FMath::Min(OutMin, S.Timeline.Samples[0].Time);
			OutMax = FMath::Max(OutMax, S.Timeline.Samples.Last().Time);
			bAny = true;
		}
		return bAny && (OutMax > OutMin);
	}

	void PaintWaiting(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2D& Size)
	{
		const FString Msg = TEXT("recording the run - a few seconds of history needed before this plots");
		PaintText(Out, Layer, G, (float)Size.X * 0.5f - TextWidth(Msg, EvoHud::FontBody()) * 0.5f,
			(float)Size.Y * 0.5f - 8.f, Msg, EvoHud::FontBody(), EvoHud::Dim);
	}

	/** Legend row: colour swatch, then text. Returns the y for the next row. */
	float PaintLegendRow(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
		float X, float Y, const FLinearColor& Swatch, const FString& Text, const FSlateFontInfo& Font)
	{
		PaintBox(Out, Layer, G, X, Y + 2.f, 8.f, 8.f, Swatch);
		PaintText(Out, Layer, G, X + 13.f, Y - 1.f, Text, Font, EvoHud::Grey);
		return Y + 13.f;
	}
}

// =============================================================================================
// SEvoTraitChart - one stat, every species, mean +/- 1 sd, whole run
// =============================================================================================
class SEvoTraitChart : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoTraitChart) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Sim = InArgs._Sim; }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(900.f, 420.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		const UEvoswarmSimSubsystem* S = Sim.Get();
		if (!S)
		{
			return LayerId;
		}
		const TArray<FSpeciesLiveStats>& Stats = S->GetSpeciesStats();

		float TMin = 0.f, TMax = 0.f;
		if (!RunTimeBounds(Stats, TMin, TMax))
		{
			PaintWaiting(Out, LayerId, Geometry, Size);
			return LayerId + 1;
		}

		const int32 Ti = StatIndex(S->GetAnalyticsTrait());

		// Y range spans the full observed min..max, so the sd band never clips.
		float VMin = TNumericLimits<float>::Max();
		float VMax = TNumericLimits<float>::Lowest();
		for (const FSpeciesLiveStats& Sp : Stats)
		{
			for (const FSpeciesTimeSample& Sample : Sp.Timeline.Samples)
			{
				if (Sample.Count <= 0.f)
				{
					continue; // an extinct species contributes zeroes; ignore them for scaling
				}
				VMin = FMath::Min(VMin, Sample.Traits[Ti].Min);
				VMax = FMath::Max(VMax, Sample.Traits[Ti].Max);
			}
		}
		if (VMax <= VMin)
		{
			VMin = FMath::Min(VMin, 0.f);
			VMax = VMin + 1.f;
		}
		const float Pad = (VMax - VMin) * 0.06f;
		VMin -= Pad;
		VMax += Pad;
		const float VRange = VMax - VMin;

		const FPlot P{ 58.f, 22.f, (float)Size.X - 58.f - 18.f, (float)Size.Y - 22.f - 26.f };
		PaintFrame(Out, LayerId, Geometry, P, VMin, VMax, TMin, TMax);
		int32 Layer = LayerId + 4;

		const float TRange = FMath::Max(TMax - TMin, KINDA_SMALL_NUMBER);
		const auto ValueToFy = [VMin, VRange](float V) { return FMath::Clamp((V - VMin) / VRange, 0.f, 1.f); };

		for (const FSpeciesLiveStats& Sp : Stats)
		{
			if (Sp.Timeline.Num() < 2)
			{
				continue;
			}
			const FLinearColor C = Sp.Color;

			// --- +/- 1 sd band, as vertical strips (Slate has no filled-polygon primitive) ---
			const FLinearColor BandColor(C.R, C.G, C.B, 0.15f);
			for (float X = P.X0; X < P.X0 + P.W; X += Evo::ChartFillStridePx)
			{
				const float Fx = (X - P.X0) / P.W;
				const FSpeciesTimeSample* Sample = SampleAtTime(Sp.Timeline, TMin + Fx * TRange);
				if (!Sample || Sample->Count <= 0.f)
				{
					continue;
				}
				const FTraitDistribution& D = Sample->Traits[Ti];
				const float YTop = P.PxY(ValueToFy(D.Mean + D.StdDev));
				const float YBot = P.PxY(ValueToFy(D.Mean - D.StdDev));
				const float W = FMath::Min(Evo::ChartFillStridePx, P.X0 + P.W - X);
				PaintBox(Out, Layer, Geometry, X, YTop, W, FMath::Max(1.f, YBot - YTop), BandColor);
			}

			// --- min / max hairlines, and the mean ---
			TArray<FVector2D> MeanPts, MinPts, MaxPts;
			const int32 N = Sp.Timeline.Num();
			MeanPts.Reserve(N); MinPts.Reserve(N); MaxPts.Reserve(N);
			for (int32 I = 0; I < N; ++I)
			{
				const FSpeciesTimeSample& Sample = Sp.Timeline.Samples[I];
				if (Sample.Count <= 0.f)
				{
					continue;
				}
				const float Fx = FMath::Clamp((Sample.Time - TMin) / TRange, 0.f, 1.f);
				MeanPts.Add(P.At(Fx, ValueToFy(Sample.Traits[Ti].Mean)));
				MinPts.Add(P.At(Fx, ValueToFy(Sample.Traits[Ti].Min)));
				MaxPts.Add(P.At(Fx, ValueToFy(Sample.Traits[Ti].Max)));
			}
			PaintLine(Out, Layer + 1, Geometry, MinPts, FLinearColor(C.R, C.G, C.B, 0.22f), 1.f);
			PaintLine(Out, Layer + 1, Geometry, MaxPts, FLinearColor(C.R, C.G, C.B, 0.22f), 1.f);
			PaintLine(Out, Layer + 2, Geometry, MeanPts, C, 1.9f);
		}

		// --- Legend, inside the top-left of the plot ---
		float LegendY = P.Y0 + 6.f;
		for (const FSpeciesLiveStats& Sp : Stats)
		{
			const FTraitDistribution& D = Sp.TraitNow[Ti];
			const FString Row = (Sp.Count > 0)
				? FString::Printf(TEXT("%s   %.2f  \x00b1 %.2f   (%d alive, %.0f..%.0f)"),
					*Sp.Name, D.Mean, D.StdDev, Sp.Count, D.Min, D.Max)
				: FString::Printf(TEXT("%s   extinct"), *Sp.Name);
			LegendY = PaintLegendRow(Out, Layer + 3, Geometry, P.X0 + 10.f, LegendY, Sp.Color, Row, EvoHud::FontSmall());
		}

		return Layer + 4;
	}

private:
	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
};

// =============================================================================================
// SEvoPopulationChart - stacked population, plus birth / death rates on a right-hand axis
// =============================================================================================
class SEvoPopulationChart : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoPopulationChart) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Sim = InArgs._Sim; }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(900.f, 420.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		const UEvoswarmSimSubsystem* S = Sim.Get();
		if (!S)
		{
			return LayerId;
		}
		const TArray<FSpeciesLiveStats>& Stats = S->GetSpeciesStats();

		float TMin = 0.f, TMax = 0.f;
		if (!RunTimeBounds(Stats, TMin, TMax))
		{
			PaintWaiting(Out, LayerId, Geometry, Size);
			return LayerId + 1;
		}
		const float TRange = FMath::Max(TMax - TMin, KINDA_SMALL_NUMBER);

		// Left axis: peak stacked total. Right axis: peak birth/death rate.
		float MaxTotal = 1.f;
		float MaxRate = 1.f;
		{
			int32 LongestN = 0;
			for (const FSpeciesLiveStats& Sp : Stats)
			{
				LongestN = FMath::Max(LongestN, Sp.Timeline.Num());
			}
			for (int32 I = 0; I < LongestN; ++I)
			{
				float Total = 0.f, Births = 0.f, Deaths = 0.f;
				for (const FSpeciesLiveStats& Sp : Stats)
				{
					if (Sp.Timeline.Samples.IsValidIndex(I))
					{
						Total += Sp.Timeline.Samples[I].Count;
						Births += Sp.Timeline.Samples[I].BirthRate;
						Deaths += Sp.Timeline.Samples[I].DeathRate;
					}
				}
				MaxTotal = FMath::Max(MaxTotal, Total);
				MaxRate = FMath::Max3(MaxRate, Births, Deaths);
			}
		}
		MaxTotal *= 1.08f;
		MaxRate *= 1.10f;

		const FPlot P{ 58.f, 22.f, (float)Size.X - 58.f - 62.f, (float)Size.Y - 22.f - 26.f };
		PaintFrame(Out, LayerId, Geometry, P, 0.f, MaxTotal, TMin, TMax);
		int32 Layer = LayerId + 4;

		// --- Stacked population area ---
		// Strips again, one per species per column, bottom-up in registration order.
		for (float X = P.X0; X < P.X0 + P.W; X += Evo::ChartFillStridePx)
		{
			const float Fx = (X - P.X0) / P.W;
			const float T = TMin + Fx * TRange;
			const float W = FMath::Min(Evo::ChartFillStridePx, P.X0 + P.W - X);
			float Base = 0.f;
			for (const FSpeciesLiveStats& Sp : Stats)
			{
				const FSpeciesTimeSample* Sample = SampleAtTime(Sp.Timeline, T);
				if (!Sample || Sample->Count <= 0.f)
				{
					continue;
				}
				const float Top = Base + Sample->Count;
				const float YTop = P.PxY(Top / MaxTotal);
				const float YBot = P.PxY(Base / MaxTotal);
				const FLinearColor C = Sp.Color;
				PaintBox(Out, Layer, Geometry, X, YTop, W, FMath::Max(1.f, YBot - YTop),
					FLinearColor(C.R * 0.85f, C.G * 0.85f, C.B * 0.85f, 0.55f));
				Base = Top;
			}
		}

		// --- Cumulative outlines, so each band's upper edge stays legible ---
		{
			const int32 NumSpecies = Stats.Num();
			TArray<TArray<FVector2D>> Outlines;
			Outlines.SetNum(NumSpecies);
			int32 LongestN = 0;
			for (const FSpeciesLiveStats& Sp : Stats)
			{
				LongestN = FMath::Max(LongestN, Sp.Timeline.Num());
			}
			for (int32 I = 0; I < LongestN; ++I)
			{
				float Base = 0.f;
				float Time = TMin;
				for (int32 Si = 0; Si < NumSpecies; ++Si)
				{
					if (!Stats[Si].Timeline.Samples.IsValidIndex(I))
					{
						continue;
					}
					const FSpeciesTimeSample& Sample = Stats[Si].Timeline.Samples[I];
					Time = Sample.Time;
					Base += Sample.Count;
					const float Fx = FMath::Clamp((Time - TMin) / TRange, 0.f, 1.f);
					Outlines[Si].Add(P.At(Fx, Base / MaxTotal));
				}
			}
			for (int32 Si = 0; Si < NumSpecies; ++Si)
			{
				PaintLine(Out, Layer + 1, Geometry, Outlines[Si], Stats[Si].Color, 1.4f);
			}
		}

		// --- Right axis: total births / deaths per minute ---
		{
			const float VStep = NiceStep(MaxRate, 5);
			for (float V = VStep; V <= MaxRate; V += VStep)
			{
				const float Y = P.PxY(V / MaxRate);
				PaintText(Out, Layer + 1, Geometry, P.X0 + P.W + 7.f, Y - 6.f,
					FormatCompact(V), EvoHud::FontTiny(), FLinearColor(0.55f, 0.6f, 0.55f, 0.8f));
			}
			PaintText(Out, Layer + 1, Geometry, P.X0 + P.W + 7.f, P.Y0 - 14.f,
				TEXT("per min"), EvoHud::FontTiny(), EvoHud::Dim);

			int32 LongestN = 0;
			for (const FSpeciesLiveStats& Sp : Stats)
			{
				LongestN = FMath::Max(LongestN, Sp.Timeline.Num());
			}
			TArray<FVector2D> BirthPts, DeathPts;
			BirthPts.Reserve(LongestN);
			DeathPts.Reserve(LongestN);
			for (int32 I = 0; I < LongestN; ++I)
			{
				float Births = 0.f, Deaths = 0.f, Time = TMin;
				bool bAny = false;
				for (const FSpeciesLiveStats& Sp : Stats)
				{
					if (Sp.Timeline.Samples.IsValidIndex(I))
					{
						Births += Sp.Timeline.Samples[I].BirthRate;
						Deaths += Sp.Timeline.Samples[I].DeathRate;
						Time = Sp.Timeline.Samples[I].Time;
						bAny = true;
					}
				}
				if (!bAny)
				{
					continue;
				}
				const float Fx = FMath::Clamp((Time - TMin) / TRange, 0.f, 1.f);
				BirthPts.Add(P.At(Fx, FMath::Clamp(Births / MaxRate, 0.f, 1.f)));
				DeathPts.Add(P.At(Fx, FMath::Clamp(Deaths / MaxRate, 0.f, 1.f)));
			}
			PaintLine(Out, Layer + 2, Geometry, BirthPts, FLinearColor(0.45f, 0.9f, 0.5f, 0.9f), 1.5f);
			PaintLine(Out, Layer + 2, Geometry, DeathPts, FLinearColor(0.95f, 0.45f, 0.45f, 0.9f), 1.5f);
		}

		// --- Legend ---
		float LegendY = P.Y0 + 6.f;
		int32 TotalPop = 0;
		for (const FSpeciesLiveStats& Sp : Stats)
		{
			TotalPop += Sp.Count;
		}
		LegendY = PaintLegendRow(Out, Layer + 3, Geometry, P.X0 + 10.f, LegendY, EvoHud::White,
			FString::Printf(TEXT("%d alive   \x2022   %d plants, %d carcasses standing"),
				TotalPop, S->GetLivePlantCount(), S->GetLiveCarcassCount()), EvoHud::FontSmall());
		for (const FSpeciesLiveStats& Sp : Stats)
		{
			LegendY = PaintLegendRow(Out, Layer + 3, Geometry, P.X0 + 10.f, LegendY, Sp.Color,
				FString::Printf(TEXT("%s   x%d   born %.1f/min, died %.1f/min"),
					*Sp.Name, Sp.Count, Evo::RatePerMinute(Sp.BirthHistory), Evo::RatePerMinute(Sp.DeathHistory)),
				EvoHud::FontSmall());
		}
		LegendY = PaintLegendRow(Out, Layer + 3, Geometry, P.X0 + 10.f, LegendY,
			FLinearColor(0.45f, 0.9f, 0.5f, 0.9f), TEXT("births / min (right axis)"), EvoHud::FontSmall());
		PaintLegendRow(Out, Layer + 3, Geometry, P.X0 + 10.f, LegendY,
			FLinearColor(0.95f, 0.45f, 0.45f, 0.9f), TEXT("deaths / min (right axis)"), EvoHud::FontSmall());

		return Layer + 4;
	}

private:
	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
};

// =============================================================================================
// SEvoScatterChart - individual genomes in 2D trait space
// =============================================================================================
class SEvoScatterChart : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoScatterChart) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Sim = InArgs._Sim; }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(900.f, 420.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		const UEvoswarmSimSubsystem* S = Sim.Get();
		if (!S)
		{
			return LayerId;
		}
		const TArray<FSpeciesLiveStats>& Stats = S->GetSpeciesStats();
		const FScatterPreset& Preset = S->GetAnalyticsScatterPreset();
		const int32 Xi = StatIndex(Preset.X);
		const int32 Yi = StatIndex(Preset.Y);

		// Range from the plotted individuals themselves, so the cloud fills the plot however
		// narrow the species' authored stat ranges happen to be.
		float XMin = TNumericLimits<float>::Max(), XMax = TNumericLimits<float>::Lowest();
		float YMin = TNumericLimits<float>::Max(), YMax = TNumericLimits<float>::Lowest();
		int32 TotalPoints = 0;
		for (const FSpeciesLiveStats& Sp : Stats)
		{
			for (const FBoidGenome& G : Sp.GenomeSamples)
			{
				XMin = FMath::Min(XMin, G.Stats[Xi]); XMax = FMath::Max(XMax, G.Stats[Xi]);
				YMin = FMath::Min(YMin, G.Stats[Yi]); YMax = FMath::Max(YMax, G.Stats[Yi]);
				++TotalPoints;
			}
		}
		if (TotalPoints == 0)
		{
			PaintWaiting(Out, LayerId, Geometry, Size);
			return LayerId + 1;
		}
		const float XPad = FMath::Max((XMax - XMin) * 0.08f, 0.05f);
		const float YPad = FMath::Max((YMax - YMin) * 0.08f, 0.05f);
		XMin -= XPad; XMax += XPad; YMin -= YPad; YMax += YPad;
		const float XRange = FMath::Max(XMax - XMin, KINDA_SMALL_NUMBER);
		const float YRange = FMath::Max(YMax - YMin, KINDA_SMALL_NUMBER);

		const FPlot P{ 58.f, 22.f, (float)Size.X - 58.f - 18.f, (float)Size.Y - 22.f - 34.f };
		PaintBox(Out, LayerId, Geometry, P.X0, P.Y0, P.W, P.H, FLinearColor(0.05f, 0.06f, 0.08f, 0.9f));

		// Grid + labels on both value axes (no time axis on this page).
		int32 Layer = LayerId + 1;
		{
			const float XStep = NiceStep(XRange, 6);
			for (float V = FMath::CeilToFloat(XMin / XStep) * XStep; V <= XMax; V += XStep)
			{
				const float X = P.PxX((V - XMin) / XRange);
				PaintBox(Out, Layer, Geometry, X, P.Y0, 1.f, P.H, EvoHud::Grid);
				const FString L = FormatCompact(V);
				PaintText(Out, Layer + 1, Geometry, X - TextWidth(L, EvoHud::FontTiny()) * 0.5f,
					P.Y0 + P.H + 4.f, L, EvoHud::FontTiny(), EvoHud::Dim);
			}
			const float YStep = NiceStep(YRange, 5);
			for (float V = FMath::CeilToFloat(YMin / YStep) * YStep; V <= YMax; V += YStep)
			{
				const float Y = P.PxY((V - YMin) / YRange);
				PaintBox(Out, Layer, Geometry, P.X0, Y, P.W, 1.f, EvoHud::Grid);
				PaintTextRight(Out, Layer + 1, Geometry, P.X0 - 6.f, Y - 6.f, FormatCompact(V), EvoHud::FontTiny(), EvoHud::Dim);
			}
			PaintBox(Out, Layer + 2, Geometry, P.X0, P.Y0 + P.H, P.W, 1.f, EvoHud::GridStrong);
			PaintBox(Out, Layer + 2, Geometry, P.X0, P.Y0, 1.f, P.H, EvoHud::GridStrong);

			// Axis titles.
			const FString XTitle = FString::Printf(TEXT("%s  \x2192"), Evo::StatLongName(Preset.X));
			PaintTextRight(Out, Layer + 2, Geometry, P.X0 + P.W, P.Y0 + P.H + 16.f, XTitle, EvoHud::FontSmall(), EvoHud::Grey);
			const FString YTitle = FString::Printf(TEXT("\x2191 %s"), Evo::StatLongName(Preset.Y));
			PaintText(Out, Layer + 2, Geometry, P.X0 + 4.f, P.Y0 - 15.f, YTitle, EvoHud::FontSmall(), EvoHud::Grey);
		}
		Layer += 3;

		// --- The individuals ---
		for (const FSpeciesLiveStats& Sp : Stats)
		{
			const FLinearColor C(Sp.Color.R, Sp.Color.G, Sp.Color.B, 0.7f);
			for (const FBoidGenome& G : Sp.GenomeSamples)
			{
				const float Fx = FMath::Clamp((G.Stats[Xi] - XMin) / XRange, 0.f, 1.f);
				const float Fy = FMath::Clamp((G.Stats[Yi] - YMin) / YRange, 0.f, 1.f);
				PaintBox(Out, Layer, Geometry, P.PxX(Fx) - 1.5f, P.PxY(Fy) - 1.5f, 3.f, 3.f, C);
			}
		}

		// --- Per-species centre and marginal spread ---
		// A cross, not an ellipse: we track each stat's sd but not the covariance between them,
		// and an ellipse would imply a correlation we never measured.
		for (const FSpeciesLiveStats& Sp : Stats)
		{
			if (Sp.Count <= 0)
			{
				continue;
			}
			const FTraitDistribution& DX = Sp.TraitNow[Xi];
			const FTraitDistribution& DY = Sp.TraitNow[Yi];
			const float CxF = FMath::Clamp((DX.Mean - XMin) / XRange, 0.f, 1.f);
			const float CyF = FMath::Clamp((DY.Mean - YMin) / YRange, 0.f, 1.f);
			const float Cx = P.PxX(CxF);
			const float Cy = P.PxY(CyF);
			const float HalfW = FMath::Max(1.f, (DX.StdDev / XRange) * P.W);
			const float HalfH = FMath::Max(1.f, (DY.StdDev / YRange) * P.H);
			const FLinearColor C(Sp.Color.R, Sp.Color.G, Sp.Color.B, 0.95f);
			PaintBox(Out, Layer + 1, Geometry, Cx - HalfW, Cy - 1.f, HalfW * 2.f, 2.f, C);
			PaintBox(Out, Layer + 1, Geometry, Cx - 1.f, Cy - HalfH, 2.f, HalfH * 2.f, C);
			PaintBox(Out, Layer + 2, Geometry, Cx - 3.f, Cy - 3.f, 6.f, 6.f, EvoHud::White);
		}

		// --- Legend ---
		float LegendY = P.Y0 + 6.f;
		for (const FSpeciesLiveStats& Sp : Stats)
		{
			LegendY = PaintLegendRow(Out, Layer + 3, Geometry, P.X0 + 10.f, LegendY, Sp.Color,
				FString::Printf(TEXT("%s   %d of %d plotted"), *Sp.Name, Sp.GenomeSamples.Num(), Sp.Count),
				EvoHud::FontSmall());
		}
		PaintLegendRow(Out, Layer + 3, Geometry, P.X0 + 10.f, LegendY, EvoHud::White,
			TEXT("cross = population mean, arms = 1 sd on each axis"), EvoHud::FontSmall());

		return Layer + 4;
	}

private:
	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
};

// =============================================================================================
// SEvoFoodWebChart - who eats what
// =============================================================================================
class SEvoFoodWebChart : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SEvoFoodWebChart) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UEvoswarmSimSubsystem>, Sim)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs) { Sim = InArgs._Sim; }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(900.f, 420.f); }

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override
	{
		const FVector2D Size = FVector2D(Geometry.GetLocalSize());
		const UEvoswarmSimSubsystem* S = Sim.Get();
		if (!S)
		{
			return LayerId;
		}
		const TArray<FSpeciesLiveStats>& Stats = S->GetSpeciesStats();
		const int32 N = Stats.Num();
		if (N == 0)
		{
			PaintWaiting(Out, LayerId, Geometry, Size);
			return LayerId + 1;
		}

		// Species are laid out vertically by average diet, so the trophic ladder emerges from
		// the genomes rather than being hard-coded: grazers at the top, hunters at the bottom.
		TArray<int32> Order;
		Order.Reserve(N);
		for (int32 I = 0; I < N; ++I)
		{
			Order.Add(I);
		}
		Order.Sort([&Stats](int32 A, int32 B) { return Stats[A].AvgDiet < Stats[B].AvgDiet; });

		const float NodeW = 148.f;
		const float NodeH = 46.f;
		const float PlantX = 24.f;
		const float SpeciesX = (float)Size.X * 0.42f - NodeW * 0.5f;
		const float CarcassX = (float)Size.X - NodeW - 24.f;
		const float TopY = 58.f;
		const float SpanY = FMath::Max((float)Size.Y - TopY - 84.f, NodeH);
		const float StepY = (N > 1) ? (SpanY - NodeH) / (N - 1) : 0.f;

		TArray<FVector2D> SpeciesCentre;
		SpeciesCentre.SetNum(N);
		for (int32 Slot = 0; Slot < N; ++Slot)
		{
			const int32 Si = Order[Slot];
			SpeciesCentre[Si] = FVector2D(SpeciesX + NodeW * 0.5f, TopY + Slot * StepY + NodeH * 0.5f);
		}
		const FVector2D PlantCentre(PlantX + NodeW * 0.5f, TopY + SpanY * 0.5f);
		const FVector2D CarcassCentre(CarcassX + NodeW * 0.5f, TopY + SpanY * 0.5f);

		// Every arrow width is scaled against the single largest flow in the diagram, so the
		// picture is comparative rather than absolute.
		float MaxFlow = KINDA_SMALL_NUMBER;
		for (int32 I = 0; I < N; ++I)
		{
			MaxFlow = FMath::Max3(MaxFlow, Stats[I].Trophic.PlantEnergy, Stats[I].Trophic.MeatEnergy);
		}
		// Kill counts and energies are different units; normalise each family separately.
		float MaxKills = KINDA_SMALL_NUMBER;
		float MaxCarcasses = KINDA_SMALL_NUMBER;
		for (int32 K = 0; K < N; ++K)
		{
			MaxCarcasses = FMath::Max(MaxCarcasses, static_cast<float>(Stats[K].Trophic.CarcassesDropped));
			for (int32 V = 0; V < N; ++V)
			{
				MaxKills = FMath::Max(MaxKills, static_cast<float>(S->GetKillCount(K, V)));
			}
		}

		int32 Layer = LayerId;

		// --- Flows ------------------------------------------------------------------------
		// Width encodes total throughput over the whole run; brightness encodes whether the
		// flow is running right now. So a thick dim arrow means "used to matter".
		for (int32 Si = 0; Si < N; ++Si)
		{
			const FSpeciesLiveStats& Sp = Stats[Si];
			const FTrophicLedger& L = Sp.Trophic;

			if (L.PlantEnergy > 0.f)
			{
				const bool bLive = Evo::RatePerMinuteF(L.PlantEnergyHistory) > 0.f;
				DrawFlow(Out, Layer, Geometry,
					FVector2D(PlantX + NodeW, PlantCentre.Y), FVector2D(SpeciesX, SpeciesCentre[Si].Y),
					Width(L.PlantEnergy, MaxFlow), FlowColor(FLinearColor(0.35f, 0.78f, 0.32f), bLive), 0.35f);
			}
			if (L.MeatEnergy > 0.f)
			{
				const bool bLive = Evo::RatePerMinuteF(L.MeatEnergyHistory) > 0.f;
				// Scavenging runs right-to-left, against the main flow, so it arcs over the top.
				DrawFlow(Out, Layer, Geometry,
					FVector2D(CarcassX, CarcassCentre.Y), FVector2D(SpeciesX + NodeW, SpeciesCentre[Si].Y),
					Width(L.MeatEnergy, MaxFlow), FlowColor(FLinearColor(0.78f, 0.42f, 0.30f), bLive), -0.55f);
			}
			if (L.CarcassesDropped > 0)
			{
				DrawFlow(Out, Layer, Geometry,
					FVector2D(SpeciesX + NodeW, SpeciesCentre[Si].Y), FVector2D(CarcassX, CarcassCentre.Y),
					Width(static_cast<float>(L.CarcassesDropped), MaxCarcasses),
					FlowColor(FLinearColor(0.55f, 0.30f, 0.34f), Sp.Count > 0), 0.28f);
			}
		}

		// Predation, killer -> victim, curving between the species nodes.
		for (int32 K = 0; K < N; ++K)
		{
			for (int32 V = 0; V < N; ++V)
			{
				const int32 Kills = S->GetKillCount(K, V);
				if (Kills <= 0)
				{
					continue;
				}
				const FLinearColor C = Stats[K].Color;
				const float Bend = (K == V) ? 0.9f : ((SpeciesCentre[K].Y < SpeciesCentre[V].Y) ? 0.45f : -0.45f);
				DrawFlow(Out, Layer + 1, Geometry,
					FVector2D(SpeciesX + NodeW * 0.82f, SpeciesCentre[K].Y),
					FVector2D(SpeciesX + NodeW * 0.18f, SpeciesCentre[V].Y),
					Width(static_cast<float>(Kills), MaxKills),
					FLinearColor(C.R, C.G, C.B, 0.75f), Bend);
			}
		}
		Layer += 2;

		// --- Nodes ------------------------------------------------------------------------
		DrawNode(Out, Layer, Geometry, PlantX, PlantCentre.Y - NodeH * 0.5f, NodeW, NodeH,
			FLinearColor(0.28f, 0.68f, 0.24f), FString(TEXT("PLANTS")),
			FString::Printf(TEXT("%d standing"), S->GetLivePlantCount()));

		int32 TotalCarcassesDropped = 0;
		for (const FSpeciesLiveStats& Sp : Stats)
		{
			TotalCarcassesDropped += Sp.Trophic.CarcassesDropped;
		}
		DrawNode(Out, Layer, Geometry, CarcassX, CarcassCentre.Y - NodeH * 0.5f, NodeW, NodeH,
			FLinearColor(0.62f, 0.24f, 0.22f), FString(TEXT("CARCASSES")),
			FString::Printf(TEXT("%d standing / %d total"), S->GetLiveCarcassCount(), TotalCarcassesDropped));

		for (int32 Slot = 0; Slot < N; ++Slot)
		{
			const int32 Si = Order[Slot];
			const FSpeciesLiveStats& Sp = Stats[Si];
			const TCHAR* DietWord = (Sp.AvgDiet < Evo::DietHerbThreshold) ? TEXT("herbivore")
				: (Sp.AvgDiet > Evo::DietCarnThreshold ? TEXT("carnivore") : TEXT("omnivore"));
			DrawNode(Out, Layer, Geometry, SpeciesX, SpeciesCentre[Si].Y - NodeH * 0.5f, NodeW, NodeH,
				Sp.Color, Sp.Name,
				FString::Printf(TEXT("x%d  %s"), Sp.Count, DietWord));
		}

		// --- Legend -----------------------------------------------------------------------
		const float LegY = (float)Size.Y - 58.f;
		float Y = LegY;
		Y = PaintLegendRow(Out, Layer + 1, Geometry, 24.f, Y, FLinearColor(0.35f, 0.78f, 0.32f),
			TEXT("grazing: plant energy absorbed"), EvoHud::FontSmall());
		Y = PaintLegendRow(Out, Layer + 1, Geometry, 24.f, Y, FLinearColor(0.78f, 0.42f, 0.30f),
			TEXT("scavenging: carcass energy absorbed (arcs back against the flow)"), EvoHud::FontSmall());
		Y = PaintLegendRow(Out, Layer + 1, Geometry, 24.f, Y, FLinearColor(0.55f, 0.30f, 0.34f),
			TEXT("corpses: kills this species suffered, which become carcasses"), EvoHud::FontSmall());
		PaintText(Out, Layer + 1, Geometry, 24.f, Y + 2.f,
			TEXT("coloured arrows between species = predation, in the killer's colour.   ")
			TEXT("Thickness = total throughput over the run; dim = not flowing right now."),
			EvoHud::FontSmall(), EvoHud::Dim);

		return Layer + 2;
	}

private:
	/** sqrt so that a flow 1% of the maximum is still a visible line rather than a hairline. */
	static float Width(float Flow, float MaxFlow)
	{
		return 1.2f + 7.f * FMath::Sqrt(FMath::Clamp(Flow / FMath::Max(MaxFlow, KINDA_SMALL_NUMBER), 0.f, 1.f));
	}

	static FLinearColor FlowColor(const FLinearColor& Base, bool bLive)
	{
		return bLive ? FLinearColor(Base.R, Base.G, Base.B, 0.95f)
			: FLinearColor(Base.R * 0.55f, Base.G * 0.55f, Base.B * 0.55f, 0.35f);
	}

	/** A bent arrow. Bend is a fraction of the span, pushed perpendicular to it. */
	static void DrawFlow(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
		const FVector2D& From, const FVector2D& To, float Thickness, const FLinearColor& C, float Bend)
	{
		const FVector2D Delta = To - From;
		const float Len = (float)Delta.Size();
		if (Len < 1.f)
		{
			return;
		}
		const FVector2D Perp = FVector2D(-Delta.Y, Delta.X) / Len;
		const FVector2D Mid = (From + To) * 0.5f + Perp * (Bend * Len * 0.35f);

		// Quadratic bezier, sampled. Slate can stroke a spline directly, but sampling into
		// MakeLines keeps us on the exact same primitive the rest of the HUD already uses.
		constexpr int32 Segments = 20;
		TArray<FVector2D> Points;
		Points.Reserve(Segments + 1);
		for (int32 I = 0; I <= Segments; ++I)
		{
			const float T = static_cast<float>(I) / Segments;
			const float U = 1.f - T;
			Points.Add(From * (U * U) + Mid * (2.f * U * T) + To * (T * T));
		}
		PaintLine(Out, Layer, G, Points, C, Thickness);

		// Arrowhead, aligned with the final segment.
		const FVector2D Tip = Points.Last();
		FVector2D Dir = Tip - Points[Points.Num() - 2];
		if (Dir.Normalize())
		{
			const FVector2D Side(-Dir.Y, Dir.X);
			const float HeadLen = 7.f + Thickness;
			const float HeadHalf = 3.f + Thickness * 0.45f;
			TArray<FVector2D> Head;
			Head.Add(Tip - Dir * HeadLen + Side * HeadHalf);
			Head.Add(Tip);
			Head.Add(Tip - Dir * HeadLen - Side * HeadHalf);
			PaintLine(Out, Layer, G, Head, C, FMath::Max(1.5f, Thickness * 0.8f));
		}
	}

	static void DrawNode(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
		float X, float Y, float W, float H, const FLinearColor& Accent,
		const FString& Title, const FString& Sub)
	{
		PaintBox(Out, Layer, G, X, Y, W, H, FLinearColor(0.06f, 0.07f, 0.09f, 0.97f));
		PaintBox(Out, Layer + 1, G, X, Y, 3.f, H, Accent);
		PaintText(Out, Layer + 2, G, X + 10.f, Y + 7.f, Title, EvoHud::FontName(), EvoHud::White);
		PaintText(Out, Layer + 2, G, X + 10.f, Y + 25.f, Sub, EvoHud::FontSmall(), EvoHud::Grey);
	}

	TWeakObjectPtr<UEvoswarmSimSubsystem> Sim;
};

// =============================================================================================
// SEvoswarmAnalytics - the page host
// =============================================================================================
void SEvoswarmAnalytics::Construct(const FArguments& InArgs)
{
	Sim = InArgs._Sim;

	// The root stays visible (and click-through) so it keeps ticking; only the content
	// collapses. Same reasoning as the inspector panel.
	SetVisibility(EVisibility::SelfHitTestInvisible);

	// One chart per page. The switcher collapses the inactive ones, so only the visible page
	// paints - which is what keeps four fairly heavy charts affordable.
	TSharedRef<SWidget> Pages =
		SNew(SWidgetSwitcher)
		.WidgetIndex(this, &SEvoswarmAnalytics::ActivePageIndex)
		+ SWidgetSwitcher::Slot()[SNew(SEvoTraitChart).Sim(Sim)]
		+ SWidgetSwitcher::Slot()[SNew(SEvoPopulationChart).Sim(Sim)]
		+ SWidgetSwitcher::Slot()[SNew(SEvoScatterChart).Sim(Sim)]
		+ SWidgetSwitcher::Slot()[SNew(SEvoFoodWebChart).Sim(Sim)];

	ChildSlot
		[
			SNew(SBox)
				.Visibility(this, &SEvoswarmAnalytics::ContentVisibility)
				[
					SNew(SBorder)
						.BorderImage(&EvoHud::WhiteBrush)
						.BorderBackgroundColor(EvoHud::OverlayBg)
						.Padding(FMargin(28.f, 22.f))
						[
							SNew(SVerticalBox)

								// Accent strip.
								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(SBox).HeightOverride(3.f)
										[SNew(SImage).Image(&EvoHud::WhiteBrush).ColorAndOpacity(EvoHud::Accent)]
								]

								// Title row: page name on the left, key hints on the right.
								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
								[
									SNew(SHorizontalBox)
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(STextBlock).Font(EvoHud::FontTitle()).ColorAndOpacity(FSlateColor(EvoHud::White))
												.Text_Lambda([this] { return PageTitleText; })
										]
										+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(16.f, 0.f, 0.f, 0.f)
										[
											SNew(STextBlock).Font(EvoHud::FontName()).ColorAndOpacity(FSlateColor(EvoHud::Accent))
												.Text_Lambda([this] { return SelectionText; })
										]
										+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
										[
											SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Dim))
												.Text(LOCTEXT("AnalyticsKeys",
													"Tab page      [ ]  selection      K  export CSV      G  close"))
										]
								]

							// Subtitle: what the current page is actually telling you.
							+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
								[
									SNew(STextBlock).Font(EvoHud::FontBody()).ColorAndOpacity(FSlateColor(EvoHud::Grey))
										.Text_Lambda([this] { return SubtitleText; })
								]

								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f)
								[EvoHud::MakeRule(FLinearColor(1, 1, 1, 0.1f))]

								// The chart itself takes all remaining height.
								+ SVerticalBox::Slot().FillHeight(1.f)
								[Pages]

								+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 4.f)
								[EvoHud::MakeRule(FLinearColor(1, 1, 1, 0.08f))]

								+ SVerticalBox::Slot().AutoHeight()
								[
									SNew(STextBlock).Font(EvoHud::FontSmall()).ColorAndOpacity(FSlateColor(EvoHud::Dim))
										.Text_Lambda([this] { return FooterText; })
								]
						]
				]
		];

	Refresh();
}

EVisibility SEvoswarmAnalytics::ContentVisibility() const
{
	const UEvoswarmSimSubsystem* S = Sim.Get();
	return (S && S->IsAnalyticsOpen()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

int32 SEvoswarmAnalytics::ActivePageIndex() const
{
	const UEvoswarmSimSubsystem* S = Sim.Get();
	return S ? static_cast<int32>(S->GetAnalyticsPage()) : 0;
}

void SEvoswarmAnalytics::Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);

	// The charts repaint themselves from live data; this only refreshes the header text.
	RefreshTimer += DeltaTime;
	if (RefreshTimer >= 0.1f)
	{
		RefreshTimer = 0.f;
		Refresh();
	}
}

void SEvoswarmAnalytics::Refresh()
{
	const UEvoswarmSimSubsystem* S = Sim.Get();
	if (!S)
	{
		return;
	}

	switch (S->GetAnalyticsPage())
	{
	case EAnalyticsPage::TraitCurves:
		PageTitleText = LOCTEXT("PageTraits", "TRAIT DRIFT");
		SelectionText = FText::FromString(FString::Printf(TEXT("\x25C0 %s \x25B6"), Evo::StatLongName(S->GetAnalyticsTrait())));
		SubtitleText = LOCTEXT("PageTraitsSub",
			"Solid line = population mean. Shaded band = \x00b1 1 standard deviation. Hairlines = the extremes alive at that moment. A band that widens then splits is a population specialising.");
		break;

	case EAnalyticsPage::Populations:
		PageTitleText = LOCTEXT("PagePop", "POPULATIONS");
		SelectionText = FText::GetEmpty();
		SubtitleText = LOCTEXT("PagePopSub",
			"Stacked areas = who makes up the ecosystem. Green and red lines (right axis) = births and deaths per minute. Where the red line crosses above the green one, a crash is already underway.");
		break;

	case EAnalyticsPage::Scatter:
	{
		const FScatterPreset& Preset = S->GetAnalyticsScatterPreset();
		PageTitleText = LOCTEXT("PageScatter", "GENOME SPACE");
		SelectionText = FText::FromString(FString::Printf(TEXT("\x25C0 %s \x00d7 %s \x25B6"),
			Evo::StatShortName(Preset.X), Evo::StatShortName(Preset.Y)));
		SubtitleText = FText::FromString(FString::Printf(
			TEXT("Every dot is one living creature - %s. Two separate clouds inside one species means it has split into two strategies, which no average can show you."),
			Preset.Story));
		break;
	}

	case EAnalyticsPage::FoodWeb:
		PageTitleText = LOCTEXT("PageWeb", "FOOD WEB");
		SelectionText = FText::GetEmpty();
		SubtitleText = LOCTEXT("PageWebSub",
			"Energy as it actually moved, in post-digestion units: a carnivore that swallows a plant badly contributes a thin arrow. Species are stacked by average diet, so the trophic ladder is emergent, not authored.");
		break;

	default:
		break;
	}

	// Footer: how much history is recorded, and at what resolution.
	float Span = 0.f;
	float Interval = Evo::StatsSampleInterval;
	int32 Samples = 0;
	for (const FSpeciesLiveStats& Sp : S->GetSpeciesStats())
	{
		if (Sp.Timeline.Num() > Samples)
		{
			Samples = Sp.Timeline.Num();
			Span = Sp.Timeline.SpanSeconds();
			Interval = Sp.Timeline.Interval;
		}
	}
	FooterText = FText::FromString(FString::Printf(
		TEXT("run %s   \x2022   %d samples spanning %s at %.1fs resolution   \x2022   K writes the whole record to Saved/Evoswarm/"),
		*EvoHud::ClockText(S->GetElapsedTime()).ToString(), Samples, *EvoHud::ClockText(Span).ToString(), Interval));
}

#undef LOCTEXT_NAMESPACE