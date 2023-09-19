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
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"MotiveMaterialLibrary",
				"Projects",
				"RenderCore",
			}
			);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"GraphEditor",
					"DerivedDataCache",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"PropertyEditor",
					"MaterialEditor",
					"EditorScriptingUtilities",
				}
				);
		}
	}
}
