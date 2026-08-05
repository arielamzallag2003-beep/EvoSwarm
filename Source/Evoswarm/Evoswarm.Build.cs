// Copyright Evoswarm.

using UnrealBuildTool;

public class Evoswarm : ModuleRules
{
	public Evoswarm(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Headers are grouped into subfolders under Public/ for clarity; this keeps them
		// includable by filename alone (e.g. "BoidStats.h") instead of by relative path.
		bLegacyPublicIncludePaths = true;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// Mass framework. MassEntity is a built-in engine Runtime module in 5.7.
			// MassCommon / MassMovement ship in the MassGameplay plugin (enabled in the .uproject).
			"MassEntity",
			"MassCommon",
			"MassMovement",
			// Code-generated terrain mesh.
			"ProceduralMeshComponent",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
