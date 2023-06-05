// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureSetsEditor : ModuleRules
{
	public TextureSetsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
				"Engine",
				"RHI",
				"UnrealEd",
				"GraphEditor",
				"TextureSets",
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
				"PropertyEditor",
				"MaterialEditor",
				"EditorScriptingUtilities",
			}
			);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				//"Engine",
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
