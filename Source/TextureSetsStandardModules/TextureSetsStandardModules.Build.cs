// (c) Electronic Arts. All Rights Reserved.

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
				"TextureSets",
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
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DerivedDataCache",
					"EditorScriptingUtilities",
					"GraphEditor",
					"MaterialEditor",
					"UnrealEd",
				}
				);
		}
	}
}
