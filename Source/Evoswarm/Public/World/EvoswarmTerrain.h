// Copyright Evoswarm.
//
// Procedural terrain + biomes as pure, deterministic functions (no actor/subsystem needed
// to query). A "ruggedness" Perlin field both classifies the biome AND drives terrain
// amplitude, so height stays continuous across biome borders (no cliffs). Boids, food and
// the terrain mesh all sample these same functions.

#pragma once

#include "CoreMinimal.h"

enum class EBiome : uint8
{
	Grassland,   // lush, flat, abundant food
	Forest,      // cover: +stealth, reduced perception, gentle hills
	Desert,      // sparse food, faster hunger, open
	Highlands    // steep, slow going, harsher metabolism
};

/** Tuning for one biome. */
struct FBiomeParams
{
	FLinearColor Color = FLinearColor::White;
	float FoodMultiplier = 1.f;       // plant spawn density weighting
	float SpeedMultiplier = 1.f;      // movement speed scale
	float HungerDrainMultiplier = 1.f;// metabolism scale
	float PerceptionMultiplier = 1.f; // sight range scale
	float StealthBonus = 0.f;         // added to a boid's effective stealth (cover)
};

namespace Evo
{
	// --- Terrain shape ---
	inline constexpr float TerrainBaseAmplitude = 500.f;   // gentle rolling base (cm)
	inline constexpr float TerrainHighlandAmplitude = 1500.f; // extra height in rugged areas
	inline constexpr float TerrainBaseFreq = 1.f / 5000.f; // base hill frequency
	inline constexpr float TerrainDetailFreq = 1.f / 2800.f;
	inline constexpr float BiomeFreq = 1.f / 12000.f;      // big organic biome patches (scaled with the 480 m arena)
	inline constexpr float SnowLine = 1050.f;              // highland terrain above this gets snow

	// --- Water + surface detail ---
	inline constexpr float SeaLevel = -260.f;   // basins below this flood into lakes/sea
	inline constexpr float BeachBand = 95.f;    // sand thickness just above the waterline
	inline constexpr float CliffNormalZ = 0.72f;// slopes steeper than this show bare rock

	/** Fractal Perlin in [0,1]. */
	inline float Fbm01(float X, float Y, float Freq, int32 Octaves, float Seed)
	{
		float Sum = 0.f;
		float Amp = 0.5f;
		float F = Freq;
		for (int32 O = 0; O < Octaves; ++O)
		{
			Sum += Amp * FMath::PerlinNoise2D(FVector2D(X * F + Seed, Y * F - Seed));
			F *= 2.f;
			Amp *= 0.5f;
		}
		return FMath::Clamp(0.5f + Sum, 0.f, 1.f);
	}

	/**
	 * Domain warp: bends the position the biome fields are sampled at with mid-scale noise,
	 * so biome contours crinkle and finger into each other organically instead of tracing the
	 * big smooth Perlin blobs (which read as rounded patches). The warp is smooth, so terrain
	 * height (which mixes in Ruggedness) stays continuous.
	 */
	inline FVector2D BiomeWarp(float X, float Y)
	{
		constexpr float WarpAmp  = 2800.f;
		constexpr float WarpFreq = 1.f / 5200.f;
		const float WX = FMath::PerlinNoise2D(FVector2D(X * WarpFreq + 311.7f, Y * WarpFreq - 97.3f));
		const float WY = FMath::PerlinNoise2D(FVector2D(X * WarpFreq - 43.9f,  Y * WarpFreq + 189.2f));
		return FVector2D(X + WX * WarpAmp, Y + WY * WarpAmp);
	}

	/** Smooth ruggedness field [0,1] — also used to classify the biome. */
	inline float Ruggedness(float X, float Y) { const FVector2D W = BiomeWarp(X, Y); return Fbm01(W.X, W.Y, BiomeFreq, 2, 47.3f); }
	/** Independent moisture field [0,1]. */
	inline float Moisture(float X, float Y)   { const FVector2D W = BiomeWarp(X, Y); return Fbm01(W.X, W.Y, BiomeFreq, 2, 28.6f); }

	inline EBiome BiomeAtAnalytic(float X, float Y)
	{
		const float R = Ruggedness(X, Y);
		if (R > 0.60f)
		{
			return EBiome::Highlands;
		}
		const float M = Moisture(X, Y);
		if (M > 0.55f) return EBiome::Forest;
		if (M > 0.32f) return EBiome::Grassland;
		return EBiome::Desert;
	}

	EBiome BiomeAtCached(float X, float Y); // defined next to the cache (EvoswarmTerrain.cpp)

	inline EBiome BiomeAt(float X, float Y)
	{
		return BiomeAtCached(X, Y);
	}

	inline FBiomeParams GetBiomeParams(EBiome Biome)
	{
		FBiomeParams P;
		switch (Biome)
		{
		case EBiome::Grassland:
			P.Color = FLinearColor(0.30f, 0.62f, 0.18f); // vivid green
			P.FoodMultiplier = 2.0f; P.SpeedMultiplier = 1.0f; P.HungerDrainMultiplier = 0.9f;
			P.PerceptionMultiplier = 1.0f; P.StealthBonus = 0.f;
			break;
		case EBiome::Forest:
			P.Color = FLinearColor(0.07f, 0.28f, 0.10f); // deep forest green
			P.FoodMultiplier = 1.2f; P.SpeedMultiplier = 0.9f; P.HungerDrainMultiplier = 1.0f;
			P.PerceptionMultiplier = 0.5f; P.StealthBonus = 4.f; // dense cover
			break;
		case EBiome::Desert:
			P.Color = FLinearColor(0.86f, 0.72f, 0.36f); // warm sand
			P.FoodMultiplier = 0.25f; P.SpeedMultiplier = 1.0f; P.HungerDrainMultiplier = 1.6f;
			P.PerceptionMultiplier = 1.3f; P.StealthBonus = 0.f; // wide open
			break;
		case EBiome::Highlands:
		default:
			P.Color = FLinearColor(0.42f, 0.40f, 0.45f); // cool grey rock
			P.FoodMultiplier = 0.5f; P.SpeedMultiplier = 0.78f; P.HungerDrainMultiplier = 1.3f;
			P.PerceptionMultiplier = 1.1f; P.StealthBonus = 0.f;
			break;
		}
		return P;
	}

	/** Analytic ground height (many Perlin octaves — slow; use TerrainHeight() instead). */
	inline float TerrainHeightAnalytic(float X, float Y)
	{
		const float Base = (Fbm01(X, Y, TerrainBaseFreq, 3, 61.5f) - 0.5f) * 2.f * TerrainBaseAmplitude;
		const float R = Ruggedness(X, Y);
		const float Detail = (Fbm01(X, Y, TerrainDetailFreq, 4, 19.2f) - 0.5f) * 2.f;
		return Base + R * R * TerrainHighlandAmplitude * Detail;
	}

	/**
	 * Baked terrain cache: the height field and biome map are pure functions of position, so
	 * they are sampled once into a grid at startup (parallel) and every runtime query becomes
	 * a bilinear (height) / nearest (biome) lookup instead of ~13 Perlin evaluations. With
	 * thousands of boids sampling the terrain several times per frame, this is one of the
	 * largest CPU wins in the sim. Read-only after Init -> safe from parallel processors.
	 */
	struct FTerrainCache
	{
		TArray<float> Heights;  // (Res+1)^2 row-major
		TArray<uint8> Biomes;   // (Res+1)^2 row-major
		int32 Res = 0;
		float HalfExtent = 0.f;
		float InvCell = 0.f;
		bool bReady = false;
	};
	EVOSWARM_API extern FTerrainCache GTerrainCache;

	/** Bakes the cache (call once at world build, before the sim starts). */
	EVOSWARM_API void InitTerrainCache(float HalfExtent, int32 Res);

	/** Continuous ground height at a world XY (cm). Cached-bilinear once the cache is baked. */
	inline float TerrainHeight(float X, float Y)
	{
		const FTerrainCache& C = GTerrainCache;
		if (C.bReady && FMath::Abs(X) < C.HalfExtent && FMath::Abs(Y) < C.HalfExtent)
		{
			const float FX = (X + C.HalfExtent) * C.InvCell;
			const float FY = (Y + C.HalfExtent) * C.InvCell;
			const int32 IX = FMath::Min(static_cast<int32>(FX), C.Res - 1);
			const int32 IY = FMath::Min(static_cast<int32>(FY), C.Res - 1);
			const float AX = FX - IX;
			const float AY = FY - IY;
			const int32 Row = C.Res + 1;
			const float* H = C.Heights.GetData() + (IY * Row + IX);
			const float Top = FMath::Lerp(H[0], H[1], AX);
			const float Bot = FMath::Lerp(H[Row], H[Row + 1], AX);
			return FMath::Lerp(Top, Bot, AY);
		}
		return TerrainHeightAnalytic(X, Y);
	}

	/** Height a creature/prop should rest at: the ground, but never below the water surface. */
	inline float SurfaceZ(float X, float Y) { return FMath::Max(TerrainHeight(X, Y), SeaLevel); }

	/**
	 * Direction (normalised XY) toward the nearest higher ground, sampled in 8 directions at the
	 * given distance. Used so a boid in water can reliably find its way to shore even when the
	 * basin floor is locally flat (where the terrain normal gives no usable gradient).
	 */
	inline FVector ToHigherGround(float X, float Y, float Dist)
	{
		static const float Dirs[8][2] = {
			{1.f, 0.f}, {0.707f, 0.707f}, {0.f, 1.f}, {-0.707f, 0.707f},
			{-1.f, 0.f}, {-0.707f, -0.707f}, {0.f, -1.f}, {0.707f, -0.707f} };
		float BestH = TerrainHeight(X, Y);
		FVector Best(0.f, 0.f, 0.f);
		for (int32 I = 0; I < 8; ++I)
		{
			const float H = TerrainHeight(X + Dirs[I][0] * Dist, Y + Dirs[I][1] * Dist);
			if (H > BestH)
			{
				BestH = H;
				Best = FVector(Dirs[I][0], Dirs[I][1], 0.f);
			}
		}
		return Best;
	}

	/** Surface normal from finite differences of the height field. */
	inline FVector TerrainNormal(float X, float Y)
	{
		const float E = 60.f;
		const float Hx = TerrainHeight(X + E, Y) - TerrainHeight(X - E, Y);
		const float Hy = TerrainHeight(X, Y + E) - TerrainHeight(X, Y - E);
		return FVector(-Hx / (2.f * E), -Hy / (2.f * E), 1.f).GetSafeNormal();
	}
}
