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

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "AnimGraph",
                "BlueprintGraph",
                "UnrealEd"
            });
        }
    }
}
