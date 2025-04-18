// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

using UnrealBuildTool;

public class TextureSets : ModuleRules
{
	public TextureSets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"ImageCore",
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
			}
			);
			
		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"TextureSetsCompiler",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"TextureSetsCompiler",
					"TextureSetsMaterialBuilder",
					"MaterialEditor",
				}
				);
		}
	}
}
