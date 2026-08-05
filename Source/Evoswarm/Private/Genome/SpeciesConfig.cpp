// Copyright Evoswarm.

#include "SpeciesConfig.h"

USpeciesConfig::USpeciesConfig()
{
	// Seed a neutral entry for every stat so designers see all rows in the editor.
	for (int32 Index = 0; Index < NumBoidStats; ++Index)
	{
		const EBoidStat Stat = static_cast<EBoidStat>(Index);
		if (!StatDefs.Contains(Stat))
		{
			StatDefs.Add(Stat, FBoidStatDef());
		}
	}
}

FBoidStatDef USpeciesConfig::GetStatDef(EBoidStat Stat) const
{
	if (const FBoidStatDef* Found = StatDefs.Find(Stat))
	{
		return *Found;
	}
	return FBoidStatDef();
}
