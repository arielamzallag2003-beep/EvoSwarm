// Copyright Evoswarm.

#include "BoidGridSubsystem.h"
#include "EvoswarmTuning.h" // ArenaHalfExtent

void UBoidGridSubsystem::EnsureGrid()
{
	if (GridDim > 0)
	{
		return;
	}
	// Pad the extent a little so look-ahead probes just past the arena edge still map into a
	// valid cell (positions are clamped into range anyway).
	const float Extent = Evo::ArenaHalfExtent * 1.05f;
	GridOrigin = -Extent;
	GridInvCell = 1.f / FMath::Max(1.f, CellSize);
	GridDim = FMath::CeilToInt((2.f * Extent) * GridInvCell) + 1;

	AgentCells.SetNum(GridDim * GridDim);
	FoodCells[0].SetNum(GridDim * GridDim);
	FoodCells[1].SetNum(GridDim * GridDim);
}

void UBoidGridSubsystem::CellCoord(const FVector& P, int32& OutX, int32& OutY) const
{
	OutX = FMath::Clamp(FMath::FloorToInt((P.X - GridOrigin) * GridInvCell), 0, GridDim - 1);
	OutY = FMath::Clamp(FMath::FloorToInt((P.Y - GridOrigin) * GridInvCell), 0, GridDim - 1);
}

int32 UBoidGridSubsystem::CellIndex(const FVector& P) const
{
	int32 IX, IY;
	CellCoord(P, IX, IY);
	return IY * GridDim + IX;
}

uint64 UBoidGridSubsystem::KeyOf(FMassEntityHandle Entity)
{
	return (static_cast<uint64>(static_cast<uint32>(Entity.Index)) << 32)
		| static_cast<uint64>(static_cast<uint32>(Entity.SerialNumber));
}

void UBoidGridSubsystem::BeginFrame()
{
	Agents.Reset();
	Foods.Reset();
	// Reset ONLY the cells populated last frame (keeps their allocations). O(occupied cells),
	// and zero heap churn in steady state.
	for (int32 Cell : TouchedAgentCells)
	{
		AgentCells[Cell].Reset();
	}
	TouchedAgentCells.Reset();
	for (int32 Type = 0; Type < 2; ++Type)
	{
		for (int32 Cell : TouchedFoodCells[Type])
		{
			FoodCells[Type][Cell].Reset();
		}
		TouchedFoodCells[Type].Reset();
	}
	ClaimedThisFrame.Reset();
}

void UBoidGridSubsystem::AddAgent(const FGridAgent& Agent)
{
	EnsureGrid();
	const int32 NewIndex = Agents.Add(Agent);
	const int32 Cell = CellIndex(Agent.Position);
	TArray<int32>& Bucket = AgentCells[Cell];
	if (Bucket.Num() == 0)
	{
		TouchedAgentCells.Add(Cell); // first occupant this frame -> remember to reset it
	}
	Bucket.Add(NewIndex);
}

void UBoidGridSubsystem::AddFood(const FGridFood& Food)
{
	EnsureGrid();
	const int32 NewIndex = Foods.Add(Food);
	const int32 Type = static_cast<int32>(Food.Type);
	const int32 Cell = CellIndex(Food.Position);
	TArray<int32>& Bucket = FoodCells[Type][Cell];
	if (Bucket.Num() == 0)
	{
		TouchedFoodCells[Type].Add(Cell);
	}
	Bucket.Add(NewIndex);
}

void UBoidGridSubsystem::QueryAgents(const FVector& Center, float Radius, TFunctionRef<void(const FGridAgent&)> Visitor) const
{
	if (GridDim == 0)
	{
		return;
	}
	const float RadiusSq = Radius * Radius;
	int32 MinX, MinY, MaxX, MaxY;
	CellCoord(Center - FVector(Radius, Radius, 0.f), MinX, MinY);
	CellCoord(Center + FVector(Radius, Radius, 0.f), MaxX, MaxY);

	for (int32 IY = MinY; IY <= MaxY; ++IY)
	{
		const int32 RowBase = IY * GridDim;
		for (int32 IX = MinX; IX <= MaxX; ++IX)
		{
			for (int32 Index : AgentCells[RowBase + IX])
			{
				const FGridAgent& Agent = Agents[Index];
				if (FVector::DistSquared(Agent.Position, Center) <= RadiusSq)
				{
					Visitor(Agent);
				}
			}
		}
	}
}

bool UBoidGridSubsystem::FindNearestFood(const FVector& Center, float Radius, EFoodType Type, FGridFood& OutFood) const
{
	if (GridDim == 0)
	{
		return false;
	}
	const float RadiusSq = Radius * Radius;
	int32 MinX, MinY, MaxX, MaxY;
	CellCoord(Center - FVector(Radius, Radius, 0.f), MinX, MinY);
	CellCoord(Center + FVector(Radius, Radius, 0.f), MaxX, MaxY);

	float BestDistSq = RadiusSq;
	bool bFound = false;
	const TArray<TArray<int32>>& Cells = FoodCells[static_cast<int32>(Type)];

	for (int32 IY = MinY; IY <= MaxY; ++IY)
	{
		const int32 RowBase = IY * GridDim;
		for (int32 IX = MinX; IX <= MaxX; ++IX)
		{
			for (int32 Index : Cells[RowBase + IX])
			{
				const FGridFood& Food = Foods[Index];
				const float DistSq = FVector::DistSquared(Food.Position, Center);
				if (DistSq <= BestDistSq && !ClaimedThisFrame.Contains(KeyOf(Food.Entity)))
				{
					BestDistSq = DistSq;
					OutFood = Food;
					bFound = true;
				}
			}
		}
	}
	return bFound;
}

bool UBoidGridSubsystem::TryClaim(FMassEntityHandle Entity)
{
	bool bAlready = false;
	ClaimedThisFrame.Add(KeyOf(Entity), &bAlready);
	return !bAlready;
}
