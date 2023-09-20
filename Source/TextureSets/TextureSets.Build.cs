// (c) Electronic Arts. All Rights Reserved.

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
				"MotiveMaterialLibrary",
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
