// Copyright Evoswarm.

#include "BoidStats.h"
#include "SpeciesConfig.h"
#include "EvoswarmTuning.h"

float FBoidGenomeLibrary::ComputeCost(const FBoidGenome& Genome, const USpeciesConfig& Species)
{
	float Cost = 0.f;
	for (int32 Index = 0; Index < NumBoidStats; ++Index)
	{
		const FBoidStatDef Def = Species.GetStatDef(static_cast<EBoidStat>(Index));
		Cost += Genome.Stats[Index] * Def.CostWeight;
	}
	return Cost;
}

void FBoidGenomeLibrary::ClampToBudget(FBoidGenome& Genome, const USpeciesConfig& Species)
{
	// 1) Hard clamp every stat to its species range.
	float MinCost = 0.f;
	float CurrentCost = 0.f;
	for (int32 Index = 0; Index < NumBoidStats; ++Index)
	{
		const FBoidStatDef Def = Species.GetStatDef(static_cast<EBoidStat>(Index));
		const float MaxV = FMath::Max(Def.Min, Def.Max);
		Genome.Stats[Index] = FMath::Clamp(Genome.Stats[Index], Def.Min, MaxV);
		MinCost += Def.Min * Def.CostWeight;
		CurrentCost += Genome.Stats[Index] * Def.CostWeight;
	}

	// 2) If we exceed the budget, shrink the discretionary (above-Min) spend uniformly.
	if (CurrentCost > Species.Budget && CurrentCost > MinCost)
	{
		const float Affordable = Species.Budget - MinCost;
		const float Discretionary = CurrentCost - MinCost;
		const float Scale = (Affordable > 0.f) ? (Affordable / Discretionary) : 0.f;
		for (int32 Index = 0; Index < NumBoidStats; ++Index)
		{
			const FBoidStatDef Def = Species.GetStatDef(static_cast<EBoidStat>(Index));
			Genome.Stats[Index] = Def.Min + (Genome.Stats[Index] - Def.Min) * Scale;
		}
	}
}

FBoidGenome FBoidGenomeLibrary::RandomWithinBudget(const USpeciesConfig& Species, FRandomStream& Rng)
{
	FBoidGenome Genome;

	float HeadroomCost[NumBoidStats];
	float SpentCost[NumBoidStats];
	float Weight[NumBoidStats];
	float CostWeight[NumBoidStats];
	float MinValue[NumBoidStats];

	float MinCost = 0.f;
	for (int32 Index = 0; Index < NumBoidStats; ++Index)
	{
		const FBoidStatDef Def = Species.GetStatDef(static_cast<EBoidStat>(Index));
		const float MaxV = FMath::Max(Def.Min, Def.Max);
		MinValue[Index] = Def.Min;
		CostWeight[Index] = Def.CostWeight;
		HeadroomCost[Index] = (MaxV - Def.Min) * Def.CostWeight;
		SpentCost[Index] = 0.f;
		Weight[Index] = Rng.FRand();
		Genome.Stats[Index] = Def.Min;
		MinCost += Def.Min * Def.CostWeight;
	}

	float Remaining = Species.Budget - MinCost;

	// Distribute the discretionary budget across stats, capped by each stat's headroom.
	for (int32 Pass = 0; Pass < 6 && Remaining > KINDA_SMALL_NUMBER; ++Pass)
	{
		float TotalWeight = 0.f;
		for (int32 Index = 0; Index < NumBoidStats; ++Index)
		{
			if (SpentCost[Index] < HeadroomCost[Index] - KINDA_SMALL_NUMBER)
			{
				TotalWeight += Weight[Index];
			}
		}
		if (TotalWeight <= KINDA_SMALL_NUMBER)
		{
			break;
		}

		float DistributedThisPass = 0.f;
		for (int32 Index = 0; Index < NumBoidStats; ++Index)
		{
			const float Room = HeadroomCost[Index] - SpentCost[Index];
			if (Room <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const float Give = FMath::Min(Remaining * (Weight[Index] / TotalWeight), Room);
			SpentCost[Index] += Give;
			DistributedThisPass += Give;
		}
		Remaining -= DistributedThisPass;
	}

	for (int32 Index = 0; Index < NumBoidStats; ++Index)
	{
		if (CostWeight[Index] > KINDA_SMALL_NUMBER)
		{
			Genome.Stats[Index] = MinValue[Index] + SpentCost[Index] / CostWeight[Index];
		}
		else
		{
			// Cost-free stats (e.g. Diet, MutationRate) don't draw from the budget, so spread
			// them randomly across their range to give the starting population real diversity.
			const FBoidStatDef Def = Species.GetStatDef(static_cast<EBoidStat>(Index));
			Genome.Stats[Index] = Rng.FRandRange(Def.Min, FMath::Max(Def.Min, Def.Max));
		}
	}

	ClampToBudget(Genome, Species);
	return Genome;
}

FBoidGenome FBoidGenomeLibrary::Mutate(const FBoidGenome& Parent, const USpeciesConfig& Species, FRandomStream& Rng)
{
	FBoidGenome Child = Parent;

	const float Rate = FMath::Clamp(Parent.Get(EBoidStat::MutationRate), 0.f, 1.f);
	for (int32 Index = 0; Index < NumBoidStats; ++Index)
	{
		const FBoidStatDef Def = Species.GetStatDef(static_cast<EBoidStat>(Index));
		const float Range = FMath::Max(Def.Max - Def.Min, 0.f);
		const float Delta = Rng.FRandRange(-1.f, 1.f) * Rate * Range;
		Child.Stats[Index] += Delta;
	}

	ClampToBudget(Child, Species);
	return Child;
}

FBoidGenome FBoidGenomeLibrary::Crossover(const FBoidGenome& ParentA, const FBoidGenome& ParentB, const USpeciesConfig& Species, FRandomStream& Rng)
{
	FBoidGenome Child;

	for (int32 Index = 0; Index < NumBoidStats; ++Index)
	{
		const float RandomRoll = Rng.FRand(); // Génère un float entre 0.f et 1.f

		if (RandomRoll < 0.40f)
		{
			// [0% à 40%] : Héritage direct du Parent A
			Child.Stats[Index] = ParentA.Stats[Index];
		}
		else if (RandomRoll < 0.80f)
		{
			// [40% à 80%] : Héritage direct du Parent B
			Child.Stats[Index] = ParentB.Stats[Index];
		}
		else if (RandomRoll < 0.99f)
		{
			// [80% à 99%] : 19% de chance d'obtenir la moyenne des deux parents (Lissage)
			Child.Stats[Index] = (ParentA.Stats[Index] + ParentB.Stats[Index]) * 0.5f;
		}
		else
		{
			// [99% à 100%] : 1% de chance d'anomalie génétique (Saut aléatoire complet)
			const FBoidStatDef Def = Species.GetStatDef(static_cast<EBoidStat>(Index));
			const float MaxV = FMath::Max(Def.Min, Def.Max);
          
			Child.Stats[Index] = Rng.FRandRange(Def.Min, MaxV);
		}
	}

	// Sécurité budgétaire : On redresse les excès si les 1% ou les moyennes ont dépassé la limite.
	ClampToBudget(Child, Species);
    
	// Optimisation : On effectue le troc de gènes à budget constant selon le MutationRate de l'enfant.
	Reallocate(Child, Species, Rng, FMath::Clamp(Child.Get(EBoidStat::MutationRate), 0.f, 1.f));
    
	return Child;
}

void FBoidGenomeLibrary::Reallocate(FBoidGenome& Genome, const USpeciesConfig& Species, FRandomStream& Rng, float MutationRate)
{
	const float Rate = FMath::Clamp(MutationRate, 0.f, 1.f);

	// Cost-free stats (Diet, MutationRate, ...) don't draw from the budget, so just jitter them
	// within their range — this is where dietary niche and evolvability drift.
	for (int32 Index = 0; Index < NumBoidStats; ++Index)
	{
		const FBoidStatDef Def = Species.GetStatDef(static_cast<EBoidStat>(Index));
		if (Def.CostWeight <= KINDA_SMALL_NUMBER)
		{
			const float Range = FMath::Max(Def.Max - Def.Min, 0.f);
			Genome.Stats[Index] = FMath::Clamp(Genome.Stats[Index] + Rng.FRandRange(-1.f, 1.f) * Rate * Range,
				Def.Min, FMath::Max(Def.Min, Def.Max));
		}
	}

	// Costed stats: move budget from a donor stat to a recipient at their respective costs, so
	// the total spent budget is conserved. Expensive stats buy little; cheap stats buy a lot.
	const int32 Steps = 1 + static_cast<int32>(Rate * Evo::ReallocStepsScale);
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const int32 Donor = Rng.RandRange(0, NumBoidStats - 1);
		const int32 Recipient = Rng.RandRange(0, NumBoidStats - 1);
		if (Donor == Recipient)
		{
			continue;
		}
		const FBoidStatDef DonorDef = Species.GetStatDef(static_cast<EBoidStat>(Donor));
		const FBoidStatDef RecipientDef = Species.GetStatDef(static_cast<EBoidStat>(Recipient));
		if (DonorDef.CostWeight <= KINDA_SMALL_NUMBER || RecipientDef.CostWeight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// Budget freeable from the donor (above its floor) and absorbable by the recipient (below its cap).
		const float DonorBudget = (Genome.Stats[Donor] - DonorDef.Min) * DonorDef.CostWeight;
		const float RecipientRoom = (FMath::Max(RecipientDef.Min, RecipientDef.Max) - Genome.Stats[Recipient]) * RecipientDef.CostWeight;
		const float Movable = FMath::Min(DonorBudget, RecipientRoom);
		if (Movable <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float MovedBudget = Movable * Rng.FRand() * Evo::ReallocStepFraction;
		Genome.Stats[Donor] -= MovedBudget / DonorDef.CostWeight;
		Genome.Stats[Recipient] += MovedBudget / RecipientDef.CostWeight;
	}

	ClampToBudget(Genome, Species);
}
