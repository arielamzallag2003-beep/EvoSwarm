// Copyright Evoswarm.

using UnrealBuildTool;
using System.Collections.Generic;

public class EvoswarmTarget : TargetRules
{
	public EvoswarmTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// V6 matches how the installed UE 5.7 engine was built (shared build env must not diverge).
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Evoswarm");
	}
}
