using UnrealBuildTool;
using System.Collections.Generic;

public class raceGPSAkronBetaEditorTarget : TargetRules
{
    public raceGPSAkronBetaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderOrderVersion.Unreal5_4;
        ExtraModuleNames.Add("raceGPSAkronBeta");
    }
}
