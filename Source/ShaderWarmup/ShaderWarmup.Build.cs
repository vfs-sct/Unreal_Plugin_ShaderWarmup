using UnrealBuildTool;

public class ShaderWarmup : ModuleRules
{
	public ShaderWarmup(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "DeveloperSettings", "UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate", "SlateCore", "RenderCore", "RHI", "AssetRegistry"
		});
	}
}
