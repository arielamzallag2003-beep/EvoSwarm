// Copyright Evoswarm.

using UnrealBuildTool;
using System.Collections.Generic;

public class EvoSwarmEditorTarget : TargetRules
{
	public EvoSwarmEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// V6 matches how the installed UE 5.7 engine was built (shared build env must not diverge).
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Evoswarm");
		ExtraModuleNames.Add("EvoSwarmFlock");
	}
}
