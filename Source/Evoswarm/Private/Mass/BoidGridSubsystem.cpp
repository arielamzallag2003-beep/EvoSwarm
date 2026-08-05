// Copyright Evoswarm.

#include "BoidGridSubsystem.h"

void UBoidGridSubsystem::BeginFrame()
{
	Agents.Reset();
	Foods.Reset();
	AgentCells.Reset();
	FoodCells.Reset();
	ClaimedThisFrame.Reset();
}

FIntPoint UBoidGridSubsystem::CellOf(const FVector& Location) const
{
	const float Inv = (CellSize > KINDA_SMALL_NUMBER) ? (1.f / CellSize) : 0.f;
	return FIntPoint(FMath::FloorToInt(Location.X * Inv), FMath::FloorToInt(Location.Y * Inv));
}

uint64 UBoidGridSubsystem::KeyOf(FMassEntityHandle Entity)
{
	return (static_cast<uint64>(static_cast<uint32>(Entity.Index)) << 32)
		| static_cast<uint64>(static_cast<uint32>(Entity.SerialNumber));
}

void UBoidGridSubsystem::AddAgent(const FGridAgent& Agent)
{
	const int32 NewIndex = Agents.Add(Agent);
	AgentCells.FindOrAdd(CellOf(Agent.Position)).Add(NewIndex);
}

void UBoidGridSubsystem::AddFood(const FGridFood& Food)
{
	const int32 NewIndex = Foods.Add(Food);
	FoodCells.FindOrAdd(CellOf(Food.Position)).Add(NewIndex);
}

void UBoidGridSubsystem::QueryAgents(const FVector& Center, float Radius, TFunctionRef<void(const FGridAgent&)> Visitor) const
{
	const float RadiusSq = Radius * Radius;
	const FIntPoint Min = CellOf(Center - FVector(Radius, Radius, 0.f));
	const FIntPoint Max = CellOf(Center + FVector(Radius, Radius, 0.f));

	for (int32 CX = Min.X; CX <= Max.X; ++CX)
	{
		for (int32 CY = Min.Y; CY <= Max.Y; ++CY)
		{
			if (const TArray<int32>* Bucket = AgentCells.Find(FIntPoint(CX, CY)))
			{
				for (int32 Index : *Bucket)
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
}

bool UBoidGridSubsystem::FindNearestFood(const FVector& Center, float Radius, EFoodType Type, FGridFood& OutFood) const
{
	const float RadiusSq = Radius * Radius;
	const FIntPoint Min = CellOf(Center - FVector(Radius, Radius, 0.f));
	const FIntPoint Max = CellOf(Center + FVector(Radius, Radius, 0.f));

	float BestDistSq = RadiusSq;
	bool bFound = false;

	for (int32 CX = Min.X; CX <= Max.X; ++CX)
	{
		for (int32 CY = Min.Y; CY <= Max.Y; ++CY)
		{
			if (const TArray<int32>* Bucket = FoodCells.Find(FIntPoint(CX, CY)))
			{
				for (int32 Index : *Bucket)
				{
					const FGridFood& Food = Foods[Index];
					if (Food.Type != Type)
					{
						continue;
					}
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
	}
	return bFound;
}

bool UBoidGridSubsystem::TryClaim(FMassEntityHandle Entity)
{
	bool bAlready = false;
	ClaimedThisFrame.Add(KeyOf(Entity), &bAlready);
	return !bAlready;
}
