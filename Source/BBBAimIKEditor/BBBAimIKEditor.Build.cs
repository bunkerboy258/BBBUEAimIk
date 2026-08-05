using UnrealBuildTool;

public class BBBAimIKEditor : ModuleRules
{
    public BBBAimIKEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "BBBAimIK",
            "AnimGraph",
            "BlueprintGraph",
            "UnrealEd"
        });
    }
}
