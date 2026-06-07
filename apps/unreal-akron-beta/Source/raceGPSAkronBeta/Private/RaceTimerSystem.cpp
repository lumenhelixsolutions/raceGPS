// Copyright raceGPS. All Rights Reserved.

#include "RaceTimerSystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void URaceTimerSystem::StartRace()
{
    bRaceActive = true;
    RaceStartTime = FPlatformTime::Seconds();
    BestLapTime = 0.0f;
    CurrentSectorTimes.Empty();
    CurrentSectorTimes.SetNum(SectorCount);
    for (int32 i = 0; i < SectorCount; ++i)
    {
        CurrentSectorTimes[i].SectorIndex = i;
        CurrentSectorTimes[i].TimeSeconds = 0.0f;
        CurrentSectorTimes[i].BestTimeSeconds = 0.0f;
    }
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race started"));
}

void URaceTimerSystem::EndRace()
{
    bRaceActive = false;
    bLapActive = false;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race ended. Best lap: %.3fs"), BestLapTime);
}

void URaceTimerSystem::StartLap()
{
    if (!bRaceActive)
    {
        StartRace();
    }
    bLapActive = true;
    CurrentLapStartTime = FPlatformTime::Seconds();
    for (int32 i = 0; i < SectorCount; ++i)
    {
        CurrentSectorTimes[i].TimeSeconds = 0.0f;
    }
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Lap started"));
}

void URaceTimerSystem::EndLap()
{
    if (!bLapActive)
        return;

    float LapTime = GetCurrentLapTime();
    bLapActive = false;

    if (BestLapTime <= 0.0f || LapTime < BestLapTime)
    {
        BestLapTime = LapTime;
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] New best lap: %.3fs"), BestLapTime);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Lap finished: %.3fs"), LapTime);
    }
}

void URaceTimerSystem::RecordSector(int32 SectorIndex)
{
    if (!bLapActive || !CurrentSectorTimes.IsValidIndex(SectorIndex))
        return;

    float Now = FPlatformTime::Seconds();
    float Elapsed = Now - CurrentLapStartTime;
    CurrentSectorTimes[SectorIndex].TimeSeconds = Elapsed;
    UpdateBestSector(SectorIndex, Elapsed);

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Sector %d: %.3fs (best: %.3fs)"),
        SectorIndex, Elapsed, CurrentSectorTimes[SectorIndex].BestTimeSeconds);
}

float URaceTimerSystem::GetCurrentLapTime() const
{
    if (!bLapActive)
        return 0.0f;
    return FPlatformTime::Seconds() - CurrentLapStartTime;
}

float URaceTimerSystem::GetSectorTime(int32 SectorIndex) const
{
    if (!CurrentSectorTimes.IsValidIndex(SectorIndex))
        return 0.0f;
    return CurrentSectorTimes[SectorIndex].TimeSeconds;
}

float URaceTimerSystem::GetBestSectorTime(int32 SectorIndex) const
{
    if (!CurrentSectorTimes.IsValidIndex(SectorIndex))
        return 0.0f;
    return CurrentSectorTimes[SectorIndex].BestTimeSeconds;
}

bool URaceTimerSystem::SaveBestTimes(const FString& TrackId)
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("track_id"), TrackId);
    Root->SetNumberField(TEXT("version"), 1);
    Root->SetNumberField(TEXT("best_lap"), BestLapTime);

    TArray<TSharedPtr<FJsonValue>> SectorArray;
    for (const FRaceSectorTime& Sector : CurrentSectorTimes)
    {
        TSharedPtr<FJsonObject> SectorObj = MakeShared<FJsonObject>();
        SectorObj->SetNumberField(TEXT("index"), Sector.SectorIndex);
        SectorObj->SetNumberField(TEXT("best_time"), Sector.BestTimeSeconds);
        SectorArray.Add(MakeShared<FJsonValueObject>(SectorObj));
    }
    Root->SetArrayField(TEXT("sectors"), SectorArray);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FString FullPath = GetRaceTimesPath(TrackId);
    FPaths::MakeStandardFilename(FullPath);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);

    bool bSuccess = FFileHelper::SaveStringToFile(Content, *FullPath);
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race times saved: %s"), *FullPath);
    }
    return bSuccess;
}

bool URaceTimerSystem::LoadBestTimes(const FString& TrackId)
{
    FString FullPath = GetRaceTimesPath(TrackId);
    FPaths::MakeStandardFilename(FullPath);

    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FullPath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, Root))
    {
        return false;
    }

    Root->TryGetNumberField(TEXT("best_lap"), BestLapTime);

    const TArray<TSharedPtr<FJsonValue>>* SectorArray;
    if (Root->TryGetArrayField(TEXT("sectors"), SectorArray))
    {
        CurrentSectorTimes.Empty();
        for (const TSharedPtr<FJsonValue>& Val : *SectorArray)
        {
            const TSharedPtr<FJsonObject>* SectorObj;
            if (!Val->TryGetObject(SectorObj))
                continue;

            FRaceSectorTime Sector;
            (*SectorObj)->TryGetNumberField(TEXT("index"), Sector.SectorIndex);
            (*SectorObj)->TryGetNumberField(TEXT("best_time"), Sector.BestTimeSeconds);
            CurrentSectorTimes.Add(Sector);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race times loaded: %s (best lap %.3fs)"),
        *FullPath, BestLapTime);
    return true;
}

FString URaceTimerSystem::GetRaceTimesPath(const FString& TrackId) const
{
    return FPaths::ProjectSavedDir() / TEXT("RaceTimes") /
        FString::Printf(TEXT("times_%s.json"), *TrackId);
}

void URaceTimerSystem::UpdateBestSector(int32 SectorIndex, float TimeSeconds)
{
    if (!CurrentSectorTimes.IsValidIndex(SectorIndex))
        return;

    float& Best = CurrentSectorTimes[SectorIndex].BestTimeSeconds;
    if (Best <= 0.0f || TimeSeconds < Best)
    {
        Best = TimeSeconds;
    }
}
