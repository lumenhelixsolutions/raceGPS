// Copyright raceGPS. All Rights Reserved.

#include "PreflightSystem.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Engine/Engine.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TArray<FPreflightCheck> UPreflightSystem::RunAllChecks()
{
    TArray<FPreflightCheck> Checks;
    Checks.Add(CheckOS());
    Checks.Add(CheckRAM());
    Checks.Add(CheckDiskSpace());
    Checks.Add(CheckGPU());
    Checks.Add(CheckCitypackIntegrity());
    Checks.Add(CheckSaveDirectory());
    Checks.Add(CheckNetwork());
    return Checks;
}

FPreflightSummary UPreflightSystem::GetSummary(const TArray<FPreflightCheck>& Checks)
{
    FPreflightSummary Summary;
    Summary.TotalChecks = Checks.Num();
    for (const auto& Check : Checks)
    {
        switch (Check.Status)
        {
        case EPreflightStatus::Pass: Summary.PassCount++; break;
        case EPreflightStatus::Warning: Summary.WarningCount++; break;
        case EPreflightStatus::Fail: Summary.FailCount++; break;
        default: break;
        }
    }
    Summary.bCanLaunch = Summary.FailCount == 0;
    Summary.RecommendedGraphicsPreset = GetRecommendedGraphicsPreset();
    return Summary;
}

FString UPreflightSystem::GetRecommendedGraphicsPreset()
{
    // Determine preset based on GPU and RAM
    FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
    int64 TotalRAM_MB = static_cast<int64>(Stats.TotalPhysical / (1024 * 1024));

    FString GPUName = FPlatformMisc::GetPrimaryGPUBrand();
    GPUName = GPUName.ToLower();

    // High-end discrete GPUs
    if (GPUName.Contains(TEXT("rtx")) || GPUName.Contains(TEXT("rx 6")) || GPUName.Contains(TEXT("rx 7")))
    {
        if (TotalRAM_MB >= 16384) return TEXT("Ultra");
        return TEXT("High");
    }
    // Mid-range discrete
    if (GPUName.Contains(TEXT("gtx")) || GPUName.Contains(TEXT("rx 5")) || GPUName.Contains(TEXT("arc")))
    {
        return TEXT("Medium");
    }
    // Integrated / low-end
    if (GPUName.Contains(TEXT("intel")) || GPUName.Contains(TEXT("uhd")) || GPUName.Contains(TEXT("vega")))
    {
        return TEXT("Low");
    }
    return TEXT("Medium");
}

bool UPreflightSystem::IsFirstRun()
{
    FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config/PlayerSettings.json");
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigPath))
    {
        return true;
    }
    FString Json;
    if (FFileHelper::LoadFileToString(Json, *ConfigPath))
    {
        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (FJsonSerializer::Deserialize(Reader, Root))
        {
            bool bOnboardingComplete = false;
            Root->TryGetBoolField(TEXT("onboarding_complete"), bOnboardingComplete);
            return !bOnboardingComplete;
        }
    }
    return true;
}

void UPreflightSystem::MarkFirstRunComplete()
{
    FString ConfigDir = FPaths::ProjectSavedDir() / TEXT("Config");
    FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*ConfigDir);
    FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config/PlayerSettings.json");
    FString Json;
    TSharedPtr<FJsonObject> Root;

    if (FFileHelper::LoadFileToString(Json, *ConfigPath))
    {
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        FJsonSerializer::Deserialize(Reader, Root);
    }
    if (!Root.IsValid())
    {
        Root = MakeShared<FJsonObject>();
    }
    Root->SetBoolField(TEXT("onboarding_complete"), true);
    Root->SetStringField(TEXT("version"), TEXT("0.2.0"));

    FString OutJson;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    FFileHelper::SaveStringToFile(OutJson, *ConfigPath);
}

FString UPreflightSystem::GetSaveDirectoryStatus()
{
    FString SaveDir = FPaths::ProjectSavedDir();
    if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*SaveDir))
    {
        if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SaveDir))
        {
            return FString::Printf(TEXT("Cannot create save directory: %s"), *SaveDir);
        }
    }
    // Test write
    FString TestFile = SaveDir / TEXT(".write_test");
    if (FFileHelper::SaveStringToFile(TEXT("test"), *TestFile))
    {
        FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*TestFile);
        return FString::Printf(TEXT("Save directory writable: %s"), *SaveDir);
    }
    return FString::Printf(TEXT("Save directory NOT writable: %s"), *SaveDir);
}

FPreflightCheck UPreflightSystem::CheckOS()
{
    FPreflightCheck Check;
    Check.Category = TEXT("Operating System");
    Check.Description = TEXT("Windows 10/11 64-bit required");

#if PLATFORM_WINDOWS
    FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
    Check.Status = EPreflightStatus::Pass;
    Check.Detail = FString::Printf(TEXT("Windows detected. Total RAM: %.1f GB"), static_cast<double>(Stats.TotalPhysicalGB));
#else
    Check.Status = EPreflightStatus::Fail;
    Check.Detail = TEXT("Unsupported operating system");
    Check.RecommendedAction = TEXT("Install Windows 10/11 64-bit");
#endif
    return Check;
}

FPreflightCheck UPreflightSystem::CheckRAM()
{
    FPreflightCheck Check;
    Check.Category = TEXT("Memory (RAM)");
    Check.Description = TEXT("8 GB minimum, 16 GB recommended");

    FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
    int64 TotalRAM_MB = Stats.TotalPhysical / (1024 * 1024);

    if (TotalRAM_MB >= 16384)
    {
        Check.Status = EPreflightStatus::Pass;
        Check.Detail = FString::Printf(TEXT("%lld MB detected (Excellent)"), TotalRAM_MB);
    }
    else if (TotalRAM_MB >= 8192)
    {
        Check.Status = EPreflightStatus::Pass;
        Check.Detail = FString::Printf(TEXT("%lld MB detected (Meets minimum)"), TotalRAM_MB);
    }
    else
    {
        Check.Status = EPreflightStatus::Fail;
        Check.Detail = FString::Printf(TEXT("%lld MB detected (Below 8 GB minimum)"), TotalRAM_MB);
        Check.RecommendedAction = TEXT("Close background applications or upgrade RAM");
    }
    return Check;
}

FPreflightCheck UPreflightSystem::CheckDiskSpace()
{
    FPreflightCheck Check;
    Check.Category = TEXT("Disk Space");
    Check.Description = TEXT("5 GB free space required");

    uint64 TotalFree = 0;
    uint64 TotalSize = 0;
    FPlatformMisc::GetDiskTotalAndFreeSpace(FPaths::ProjectDir(), TotalSize, TotalFree);
    int64 FreeMB = TotalFree / (1024 * 1024);

    if (FreeMB >= 10240)
    {
        Check.Status = EPreflightStatus::Pass;
        Check.Detail = FString::Printf(TEXT("%lld MB free (Plenty)"), FreeMB);
    }
    else if (FreeMB >= 5120)
    {
        Check.Status = EPreflightStatus::Pass;
        Check.Detail = FString::Printf(TEXT("%lld MB free (Meets minimum)"), FreeMB);
    }
    else
    {
        Check.Status = EPreflightStatus::Fail;
        Check.Detail = FString::Printf(TEXT("%lld MB free (Need 5 GB+)"), FreeMB);
        Check.RecommendedAction = TEXT("Free up disk space or install to another drive");
    }
    return Check;
}

FPreflightCheck UPreflightSystem::CheckGPU()
{
    FPreflightCheck Check;
    Check.Category = TEXT("Graphics Card");
    Check.Description = TEXT("DirectX 12 compatible GPU required");

    FString GPUName = FPlatformMisc::GetPrimaryGPUBrand();
    FString GPUDriver = TEXT("Unknown");

    if (GPUName.IsEmpty() || GPUName == TEXT("Unknown"))
    {
        Check.Status = EPreflightStatus::Warning;
        Check.Detail = TEXT("GPU detection failed");
        Check.RecommendedAction = TEXT("Update graphics drivers");
    }
    else
    {
        Check.Status = EPreflightStatus::Pass;
        Check.Detail = FString::Printf(TEXT("%s (Driver: %s)"), *GPUName, *GPUDriver);
    }
    return Check;
}

FPreflightCheck UPreflightSystem::CheckCitypackIntegrity()
{
    FPreflightCheck Check;
    Check.Category = TEXT("Citypack Data");
    Check.Description = TEXT("Akron citypack must be present and valid");

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    FString ManifestPath = FPaths::ProjectContentDir() / TEXT("../citypacks/akron-oh-beta-001/akron_semantic_manifest.json");
    FString RoutesPath = FPaths::ProjectContentDir() / TEXT("../citypacks/akron-oh-beta-001/akron_routes.json");
    FString LevelSpecPath = FPaths::ProjectContentDir() / TEXT("../../generated/AkronWorld_LevelSpec.json");
    if (!PlatformFile.FileExists(*ManifestPath))
    {
        Check.Status = EPreflightStatus::Fail;
        Check.Detail = TEXT("Akron citypack manifest not found");
        Check.RecommendedAction = TEXT("Verify game installation or re-install citypack");
    }
    else if (!PlatformFile.FileExists(*RoutesPath))
    {
        Check.Status = EPreflightStatus::Fail;
        Check.Detail = TEXT("Akron route data not found");
        Check.RecommendedAction = TEXT("Regenerate or restore akron_routes.json before launching");
    }
    else if (!PlatformFile.FileExists(*LevelSpecPath))
    {
        Check.Status = EPreflightStatus::Fail;
        Check.Detail = TEXT("Canonical AkronWorld level spec not found");
        Check.RecommendedAction = TEXT("Run tools/generate-level-spec.py to regenerate AkronWorld_LevelSpec.json");
    }
    else
    {
        Check.Status = EPreflightStatus::Pass;
        Check.Detail = TEXT("Akron manifest, route data, and canonical level spec found");
    }
    return Check;
}

FPreflightCheck UPreflightSystem::CheckSaveDirectory()
{
    FPreflightCheck Check;
    Check.Category = TEXT("Save Directory");
    Check.Description = TEXT("Save directory must be writable");

    FString Status = GetSaveDirectoryStatus();
    if (Status.StartsWith(TEXT("Save directory writable:")))
    {
        Check.Status = EPreflightStatus::Pass;
        Check.Detail = Status;
    }
    else
    {
        Check.Status = EPreflightStatus::Fail;
        Check.Detail = Status;
        Check.RecommendedAction = TEXT("Run game as Administrator or change save location");
    }
    return Check;
}

FPreflightCheck UPreflightSystem::CheckNetwork()
{
    FPreflightCheck Check;
    Check.Category = TEXT("Network");
    Check.Description = TEXT("Optional: for leaderboards and updates");

    // Network is optional, so we just warn if offline
    Check.Status = EPreflightStatus::Pass;
    Check.Detail = TEXT("Network check skipped (leaderboards optional)");
    Check.RecommendedAction = TEXT("");
    return Check;
}
