// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

using UnrealBuildTool;

public class TextureSetsMaterialBuilder : ModuleRules
{
	public TextureSetsMaterialBuilder(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"ImageCore",
				"TextureSetsCommon",
				"MaterialEditor",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Projects",
				"RenderCore",
				"EditorScriptingUtilities",
				"GraphEditor",
				"UnrealEd",
			}
			);
	}
}
