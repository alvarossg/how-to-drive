using UnrealBuildTool;

public class HowToMechanicTarget : TargetRules
{
	public HowToMechanicTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("HowToMechanic");
	}
}
