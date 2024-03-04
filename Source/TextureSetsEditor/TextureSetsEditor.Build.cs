// (c) Electronic Arts. All Rights Reserved.

using UnrealBuildTool;

public class TextureSetsEditor : ModuleRules
{
	public TextureSetsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"TextureSetsCommon",
				"TextureSets",
				"UnrealEd",
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MaterialEditor",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus",
			}
			);
	}
}
