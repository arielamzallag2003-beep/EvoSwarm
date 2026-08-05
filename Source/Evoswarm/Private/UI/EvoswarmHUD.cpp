// Copyright Evoswarm.

#include "EvoswarmHUD.h"
#include "EvoswarmSimSubsystem.h"
#include "EvoswarmTuning.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"

void AEvoswarmHUD::DrawHUD()
{
	Super::DrawHUD();

	UWorld* World = GetWorld();
	UEvoswarmSimSubsystem* Sim = World ? World->GetSubsystem<UEvoswarmSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}

	const TArray<FSpeciesLiveStats>& Stats = Sim->GetSpeciesStats();

	UFont* Large = GEngine ? GEngine->GetLargeFont() : nullptr;
	UFont* Medium = GEngine ? GEngine->GetMediumFont() : nullptr;
	UFont* Small = GEngine ? GEngine->GetSmallFont() : nullptr;

	// Layout.
	const float PanelX = 24.f;
	const float PanelY = 24.f;
	const float PanelW = 494.f;
	const float HeaderH = 76.f;
	const float RowH = 100.f;
	const float PanelH = HeaderH + Stats.Num() * RowH + 12.f;

	const FLinearColor PanelBg(0.02f, 0.025f, 0.04f, 0.82f);
	const FLinearColor White(0.92f, 0.94f, 0.97f, 1.f);
	const FLinearColor Grey(0.65f, 0.68f, 0.72f, 1.f);
	const FLinearColor BarBg(0.12f, 0.13f, 0.16f, 1.f);
	const FLinearColor Accent(0.30f, 0.65f, 1.0f, 1.f);

	// Panel + top accent strip.
	DrawRect(PanelBg, PanelX, PanelY, PanelW, PanelH);
	DrawRect(Accent, PanelX, PanelY, PanelW, 3.f);

	// Header.
	int32 TotalPop = 0;
	int32 PeakGen = 0;
	for (const FSpeciesLiveStats& S : Stats)
	{
		TotalPop += S.Count;
		PeakGen = FMath::Max(PeakGen, S.MaxGeneration);
	}
	const float Dt = World->GetDeltaSeconds();
	const float Fps = (Dt > 0.f) ? (1.f / Dt) : 0.f;
	const FLinearColor FpsColor = (Fps >= 50.f) ? FLinearColor(0.45f, 0.9f, 0.5f, 1.f)
		: (Fps >= 30.f) ? FLinearColor(0.95f, 0.8f, 0.35f, 1.f) : FLinearColor(0.95f, 0.4f, 0.4f, 1.f);

	const float Elapsed = Sim->GetElapsedTime();
	const int32 Mins = FMath::FloorToInt(Elapsed / 60.f);
	const int32 Secs = FMath::FloorToInt(Elapsed) % 60;

	DrawText(TEXT("EVOSWARM"), White, PanelX + 16.f, PanelY + 12.f, Large);
	DrawText(FString::Printf(TEXT("Boids %d    Food %d    Gen %d    %d:%02d"), TotalPop, Sim->GetFoodCount(), PeakGen, Mins, Secs),
		Grey, PanelX + 16.f, PanelY + 46.f, Small);
	DrawText(FString::Printf(TEXT("%.0f FPS"), Fps), FpsColor, PanelX + PanelW - 78.f, PanelY + 46.f, Small);

	// Shared vertical scale across all species' population histories, so curves are comparable.
	int32 HistMax = 1;
	for (const FSpeciesLiveStats& S : Stats)
	{
		for (int32 V : S.PopHistory)
		{
			HistMax = FMath::Max(HistMax, V);
		}
	}

	float RowY = PanelY + HeaderH;
	const float GraphX = PanelX + 16.f;
	const float GraphW = PanelW - 32.f;
	const float GraphTop = 20.f;   // offset from RowY
	const float GraphH = 16.f;

	for (const FSpeciesLiveStats& S : Stats)
	{
		// Colour swatch + name + generation + count.
		DrawRect(S.Color, PanelX + 16.f, RowY + 2.f, 14.f, 14.f);
		DrawText(S.Name, S.Color, PanelX + 38.f, RowY - 2.f, Medium);
		DrawText(FString::Printf(TEXT("gen %.0f / %d"), S.AvgGeneration, S.MaxGeneration), Grey, PanelX + 150.f, RowY + 2.f, Small);
		DrawText(FString::Printf(TEXT("x %d"), S.Count), White, PanelX + PanelW - 74.f, RowY - 2.f, Medium);

		// Population-over-time sparkline, with the peak value labelled.
		const float GY = RowY + GraphTop;
		DrawRect(BarBg, GraphX, GY, GraphW, GraphH);
		const int32 NumPts = S.PopHistory.Num();
		int32 PeakPop = S.Count;
		if (NumPts >= 2)
		{
			for (int32 I = 1; I < NumPts; ++I)
			{
				PeakPop = FMath::Max(PeakPop, S.PopHistory[I]);
				const float X0 = GraphX + GraphW * (static_cast<float>(I - 1) / (NumPts - 1));
				const float X1 = GraphX + GraphW * (static_cast<float>(I) / (NumPts - 1));
				const float Y0 = GY + GraphH - GraphH * FMath::Clamp(static_cast<float>(S.PopHistory[I - 1]) / HistMax, 0.f, 1.f);
				const float Y1 = GY + GraphH - GraphH * FMath::Clamp(static_cast<float>(S.PopHistory[I]) / HistMax, 0.f, 1.f);
				DrawLine(X0, Y0, X1, Y1, S.Color, 1.5f);
			}
		}
		DrawText(FString::Printf(TEXT("peak %d"), PeakPop), Grey, GraphX + GraphW - 56.f, GY + 1.f, Small);

		// Diet gradient bar (herbivore green -> carnivore red) with a marker at this species'
		// average diet — the same colour scale the creatures themselves are drawn with.
		const float DietY = RowY + 42.f;
		const float DietH = 9.f;
		const int32 Segs = 24;
		for (int32 Seg = 0; Seg < Segs; ++Seg)
		{
			const float D = (Seg + 0.5f) / Segs;
			DrawRect(Evo::DietColor(D), GraphX + GraphW * (static_cast<float>(Seg) / Segs), DietY, GraphW / Segs + 1.f, DietH);
		}
		const float MarkerX = GraphX + GraphW * FMath::Clamp(S.AvgDiet, 0.f, 1.f);
		DrawRect(White, MarkerX - 1.5f, DietY - 3.f, 3.f, DietH + 6.f);

		// Averaged genome values (watch these drift as the species evolves).
		const TCHAR* DietWord = (S.AvgDiet < 0.35f) ? TEXT("herbivore") : (S.AvgDiet > 0.65f ? TEXT("carnivore") : TEXT("omnivore"));
		DrawText(FString::Printf(TEXT("HP %.0f   Spd %.0f   Per %.0f   Life %.0f"),
			S.AvgHP, S.AvgWalkSpeed, S.AvgPerception, S.AvgLifespan), Grey, PanelX + 16.f, RowY + 56.f, Small);
		DrawText(FString::Printf(TEXT("Dmg %.0f   Arm %.0f   Mut %.2f   Diet %.2f (%s)"),
			S.AvgDamage, S.AvgArmor, S.AvgMutationRate, S.AvgDiet, DietWord), Grey, PanelX + 16.f, RowY + 74.f, Small);

		RowY += RowH;
	}
}
