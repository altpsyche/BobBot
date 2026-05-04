// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BobBot : ModuleRules
{
	public BobBot(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		// Niagara module headers use C++20 features (default-initialized bit-fields).
		CppStandard = CppStandardVersion.Cpp20;
		
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
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"LevelEditor",
				"Kismet",
				"KismetCompiler",
				"BlueprintGraph",
				"EditorScriptingUtilities",
				"PythonScriptPlugin",
				"Json",
				"JsonUtilities",
				"ApplicationCore",
				"ImageWrapper",
				"Niagara",
				"MovieScene",
				"MovieSceneTracks",
				"UMG",
				"UMGEditor",
				"AIModule",
				"EnhancedInput",
				"Landscape",
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
