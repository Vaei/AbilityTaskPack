// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AbilityTaskPack : ModuleRules
{
	public AbilityTaskPack(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayAbilities",
				"GameplayTasks",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"TargetingSystem",
			}
			);
	}
}
