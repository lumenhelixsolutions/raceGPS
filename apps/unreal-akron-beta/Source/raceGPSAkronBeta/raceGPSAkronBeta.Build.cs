using UnrealBuildTool;

public class raceGPSAkronBeta : ModuleRules
{
    public raceGPSAkronBeta(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp17;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "ChaosVehicles",
            "ChaosVehiclesCore",
            "EnhancedInput",
            "UMG",
            "Slate",
            "SlateCore",
            "Json",
            "JsonUtilities",
            "XmlParser",
            "Projects",
            "RenderCore",
            "RHI",
            "ProceduralMeshComponent",
            "OnlineSubsystem",
            "OnlineSubsystemUtils",
            "Niagara",
            "NiagaraCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects"
        });

        PublicIncludePaths.AddRange(new string[]
        {
            ModuleDirectory + "/Public"
        });

        PrivateIncludePaths.AddRange(new string[]
        {
            ModuleDirectory + "/Private"
        });
    }
}
