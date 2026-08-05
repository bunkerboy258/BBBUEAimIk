using UnrealBuildTool;

public class BBBAimIK : ModuleRules
{
    public BBBAimIK(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AnimGraphRuntime",
            "AnimationCore"
        });

    }
}
