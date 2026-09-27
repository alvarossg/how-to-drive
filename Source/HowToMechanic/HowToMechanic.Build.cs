using UnrealBuildTool;

public class HowToMechanic : ModuleRules
{
	public HowToMechanic(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Las carpetas de Source son raíces de include: #include "Core/HTMTypes.h", etc.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"PhysicsCore",
			"NetCore",
			"DeveloperSettings",
			"Niagara",
			"UMG",
			"Slate",
			"SlateCore",
			"Json",
			"JsonUtilities",
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});

		// Steam se carga en tiempo de ejecución según DefaultEngine.ini.
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemNull");
	}
}
