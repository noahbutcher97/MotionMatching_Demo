// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class MotionMatching_Demo : ModuleRules
{
	public MotionMatching_Demo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicIncludePaths.AddRange(
			new string[] {
				"MotionMatching_Demo/Public",
				// Add plugin public includes
			}
		);
		
		PrivateIncludePaths.AddRange(
			new string[] {
				"MotionMatching_Demo/Private"
			}
		);
		
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"PoseSearch", 
			"AnimGraphRuntime", 
			"MotionTrajectory",
			"DeveloperSettings",  // <-- ADDED: Required for UFuzzyMatchingSettings
			"ReasoningEngine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { 
			"EnhancedInput"  // <-- FIXED: Removed duplicate entries
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"EditorSubsystem",
				"EditorWidgets",
				"Blutility",           // For editor utilities
				"UMG",                 // For UUserWidget
				"UMGEditor"            // For EditorUtilityWidget
			});
		}
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}