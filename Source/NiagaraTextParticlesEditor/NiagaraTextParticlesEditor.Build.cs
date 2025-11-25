// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraTextParticlesEditor : ModuleRules
{
	public NiagaraTextParticlesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"NiagaraTextParticles",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"Niagara",
                "NiagaraEditor",
                "Slate",
				"SlateCore",
				"AssetTools",
				"AssetRegistry",
				"ContentBrowser",
			}
		);
	}
}


