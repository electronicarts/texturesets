// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

using UnrealBuildTool;

public class TextureSetsCompiler : ModuleRules
{
	public TextureSetsCompiler(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"DerivedDataCache",
				"TextureSetsCommon",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Projects",
				"RenderCore",
				"ImageCore",
			}
			);
	}
}
