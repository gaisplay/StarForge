// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StarForge : ModuleRules
{
	public StarForge(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"StarForgeCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
