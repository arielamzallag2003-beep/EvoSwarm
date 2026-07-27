// Copyright Evoswarm.
//
// The HUD's shared look: one white brush, one palette, one font ramp, and the couple of
// tiny widget helpers both panels need. This used to be an anonymous namespace inside
// SEvoswarmHUD.cpp; it moved here when the analytics overlay became a second file, so the
// two panels cannot drift apart. Still code-only - no assets, no UMG, no Slate style set.

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

namespace EvoHud
{
	/** 1x1 solid white, tinted at every use site. One instance shared by every widget. */
	inline const FSlateColorBrush WhiteBrush(FLinearColor::White);

	// --- Palette ---
	inline const FLinearColor PanelBg(0.02f, 0.025f, 0.04f, 0.86f);
	inline const FLinearColor OverlayBg(0.015f, 0.02f, 0.032f, 0.96f);  // analytics: near-opaque
	inline const FLinearColor White(0.92f, 0.94f, 0.97f, 1.f);
	inline const FLinearColor Grey(0.65f, 0.68f, 0.72f, 1.f);
	inline const FLinearColor Dim(0.42f, 0.45f, 0.50f, 1.f);
	inline const FLinearColor BarBg(0.12f, 0.13f, 0.16f, 1.f);
	inline const FLinearColor Accent(0.30f, 0.65f, 1.0f, 1.f);
	inline const FLinearColor Good(0.55f, 0.85f, 0.55f, 1.f);
	inline const FLinearColor Bad(0.85f, 0.55f, 0.55f, 1.f);
	inline const FLinearColor Grid(1.f, 1.f, 1.f, 0.07f);       // chart gridlines
	inline const FLinearColor GridStrong(1.f, 1.f, 1.f, 0.16f);  // chart axes

	// --- Fonts ---
	inline FSlateFontInfo FontTitle() { return FCoreStyle::GetDefaultFontStyle("Bold", 15); }
	inline FSlateFontInfo FontName() { return FCoreStyle::GetDefaultFontStyle("Bold", 11); }
	inline FSlateFontInfo FontBody() { return FCoreStyle::GetDefaultFontStyle("Regular", 9); }
	inline FSlateFontInfo FontSmall() { return FCoreStyle::GetDefaultFontStyle("Regular", 8); }
	inline FSlateFontInfo FontTiny() { return FCoreStyle::GetDefaultFontStyle("Regular", 7); }

	// --- Layout ---
	inline constexpr float PanelWidth = 500.f;
	inline constexpr float InspectWidth = 310.f;

	/** m:ss for sim clocks and chart time axes. */
	inline FText ClockText(float Seconds)
	{
		const int32 M = FMath::FloorToInt(Seconds / 60.f);
		const int32 S = FMath::FloorToInt(Seconds) % 60;
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), M, S));
	}

	/** A 1px horizontal separator. */
	inline TSharedRef<SWidget> MakeRule(const FLinearColor& Color, float Height = 1.f)
	{
		return SNew(SBox).HeightOverride(Height)
			[
				SNew(SImage).Image(&WhiteBrush).ColorAndOpacity(Color)
			];
	}
}