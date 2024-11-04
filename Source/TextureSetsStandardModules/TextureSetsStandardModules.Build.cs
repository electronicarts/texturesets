// Copyright (c) 2024 Electronic Arts. All Rights Reserved.

using UnrealBuildTool;

public class TextureSetsStandardModules : ModuleRules
{
	public TextureSetsStandardModules(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"ImageCore",
				"DerivedDataCache",
				"EditorScriptingUtilities",
				"GraphEditor",
				"MaterialEditor",
				"UnrealEd",
				"TextureSetsCommon",
				"TextureSets",
				"TextureSetsCompiler",
				"TextureSetsMaterialBuilder",
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
	}
}
