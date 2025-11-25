using UnrealBuildTool;

public class NiagaraTextParticlesVertexFactories : ModuleRules
{
	public NiagaraTextParticlesVertexFactories(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"RenderCore",
				"RHI",
				"Niagara",
				"NiagaraShader",
				"NiagaraVertexFactories",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Projects",
			}
		);
	}
}


