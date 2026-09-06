using UnrealBuildTool;

public class StarForgeCore : ModuleRules
{
	public StarForgeCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Intentionally no "Engine" dependency: this module must stay unit-testable
		// without spinning up an editor or a world.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject"
		});
	}
}
