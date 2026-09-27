using UnrealBuildTool;

public class HowToMechanicEditorTarget : TargetRules
{
	public HowToMechanicEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("HowToMechanic");
	}
}
