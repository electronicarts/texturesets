// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureSets : ModuleRules
{
	public TextureSets(ReadOnlyTargetRules Target) : base(Target)
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
				"UnrealEd",
				"GraphEditor",
				"RHI",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"PropertyEditor",
				"MaterialEditor",
				"EditorScriptingUtilities",
				"MotiveMaterialLibrary",
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
