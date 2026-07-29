using UnrealBuildTool;

public class EvoSwarmFlock : ModuleRules
{
    public EvoSwarmFlock(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivatePCHHeaderFile = "EvoSwarmFlock.h";

        // This module keeps its sources at the module root (no Public/Private
        // split); UE 5.7 no longer adds the root to the include path implicitly.
        PrivateIncludePaths.Add(ModuleDirectory);

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",   // UBoidSelectionSubsystem mouse/keyboard input
            "Slate",           // FSlateRect used in screen-rect box selection
            "SlateCore",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // Add "MassEntity" here if you switch to Unreal Mass ECS
        });

        // Allow the compiler to fully inline behaviour free functions.
        // In Development builds this is already the default; this makes it
        // explicit for Shipping as well.
        bUseUnity = false;
        OptimizeCode = CodeOptimization.InShippingBuildsOnly;
    }
}
