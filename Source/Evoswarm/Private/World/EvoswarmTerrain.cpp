// Copyright Evoswarm.
//
// Baked terrain cache: samples the analytic Perlin height/biome fields once (in parallel)
// so the thousands of per-frame terrain queries from the Mass processors become cheap
// array lookups. See the declaration notes in EvoswarmTerrain.h.

#include "EvoswarmTerrain.h"
#include "Async/ParallelFor.h"

namespace Evo
{
	FTerrainCache GTerrainCache;
	float GDaylight = 1.f; // full day until the sky clock updates it
	float GSkyHour = 12.f;

	void InitTerrainCache(float HalfExtent, int32 Res)
	{
		FTerrainCache& C = GTerrainCache;
		C.bReady = false; // guard: samplers fall back to analytic while (re)baking
		C.Res = Res;
		C.HalfExtent = HalfExtent;
		C.InvCell = static_cast<float>(Res) / (2.f * HalfExtent);

		const int32 RowLen = Res + 1;
		C.Heights.SetNumUninitialized(RowLen * RowLen);
		C.Biomes.SetNumUninitialized(RowLen * RowLen);

		const float Cell = (2.f * HalfExtent) / static_cast<float>(Res);
		ParallelFor(RowLen, [&C, HalfExtent, Cell, RowLen](int32 IY)
		{
			const float Y = -HalfExtent + IY * Cell;
			float* HRow = C.Heights.GetData() + IY * RowLen;
			uint8* BRow = C.Biomes.GetData() + IY * RowLen;
			for (int32 IX = 0; IX < RowLen; ++IX)
			{
				const float X = -HalfExtent + IX * Cell;
				HRow[IX] = TerrainHeightAnalytic(X, Y);
				BRow[IX] = static_cast<uint8>(BiomeAtAnalytic(X, Y));
			}
		});

		C.bReady = true;
	}

	EBiome BiomeAtCached(float X, float Y)
	{
		const FTerrainCache& C = GTerrainCache;
		if (C.bReady && FMath::Abs(X) < C.HalfExtent && FMath::Abs(Y) < C.HalfExtent)
		{
			const int32 IX = FMath::Clamp(FMath::RoundToInt((X + C.HalfExtent) * C.InvCell), 0, C.Res);
			const int32 IY = FMath::Clamp(FMath::RoundToInt((Y + C.HalfExtent) * C.InvCell), 0, C.Res);
			return static_cast<EBiome>(C.Biomes[IY * (C.Res + 1) + IX]);
		}
		return BiomeAtAnalytic(X, Y);
	}
}
