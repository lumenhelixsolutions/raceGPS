#include "LeaderboardSystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Math/UnrealMathUtility.h"

void ULeaderboardSystem::AddEntry(const FString& RouteId, const FLeaderboardEntry& Entry)
{
    TArray<FLeaderboardEntry>& Entries = Leaderboards.FindOrAdd(RouteId);
    Entries.Add(Entry);
    SortEntries(Entries);

    // Keep top 100
    if (Entries.Num() > 100)
    {
        Entries.SetNum(100);
    }

    SaveLeaderboard(RouteId);
}

TArray<FLeaderboardEntry> ULeaderboardSystem::GetTopEntries(const FString& RouteId, int32 Count) const
{
    const TArray<FLeaderboardEntry>* Entries = Leaderboards.Find(RouteId);
    if (!Entries)
        return TArray<FLeaderboardEntry>();

    return TArray<FLeaderboardEntry>(Entries->GetData(), FMath::Min(Count, Entries->Num()));
}

int32 ULeaderboardSystem::GetPlayerRank(const FString& RouteId, float TimeSeconds) const
{
    const TArray<FLeaderboardEntry>* Entries = Leaderboards.Find(RouteId);
    if (!Entries)
        return 1;

    for (int32 i = 0; i < Entries->Num(); ++i)
    {
        if (TimeSeconds <= Entries->operator[](i).TimeSeconds)
        {
            return i + 1;
        }
    }
    return Entries->Num() + 1;
}

bool ULeaderboardSystem::SaveLeaderboard(const FString& RouteId)
{
    const TArray<FLeaderboardEntry>* Entries = Leaderboards.Find(RouteId);
    if (!Entries)
        return false;

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("route_id"), RouteId);
    Root->SetNumberField(TEXT("version"), 1);

    TArray<TSharedPtr<FJsonValue>> EntryArray;
    for (const FLeaderboardEntry& Entry : *Entries)
    {
        TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
        EntryObj->SetStringField(TEXT("name"), Entry.PlayerName);
        EntryObj->SetNumberField(TEXT("time"), Entry.TimeSeconds);
        EntryObj->SetStringField(TEXT("medal"), Entry.Medal);
        EntryObj->SetStringField(TEXT("date"), Entry.Date);
        EntryObj->SetStringField(TEXT("vehicle"), Entry.VehicleUsed);
        EntryObj->SetNumberField(TEXT("collisions"), Entry.Collisions);
        EntryObj->SetBoolField(TEXT("is_player"), Entry.bIsPlayer);
        EntryArray.Add(MakeShared<FJsonValueObject>(EntryObj));
    }
    Root->SetArrayField(TEXT("entries"), EntryArray);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FString FullPath = GetLeaderboardPath(RouteId);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);

    bool bSuccess = FFileHelper::SaveStringToFile(Content, *FullPath);
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Leaderboard saved: %s (%d entries)"), *FullPath, Entries->Num());
    }
    return bSuccess;
}

bool ULeaderboardSystem::LoadLeaderboard(const FString& RouteId)
{
    FString FullPath = GetLeaderboardPath(RouteId);

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

    TArray<FLeaderboardEntry>& Entries = Leaderboards.FindOrAdd(RouteId);
    Entries.Empty();

    const TArray<TSharedPtr<FJsonValue>>* EntryArray;
    if (Root->TryGetArrayField(TEXT("entries"), EntryArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *EntryArray)
        {
            const TSharedPtr<FJsonObject>* EntryObj;
            if (!Val->TryGetObject(EntryObj))
                continue;

            FLeaderboardEntry Entry;
            (*EntryObj)->TryGetStringField(TEXT("name"), Entry.PlayerName);
            (*EntryObj)->TryGetNumberField(TEXT("time"), Entry.TimeSeconds);
            (*EntryObj)->TryGetStringField(TEXT("medal"), Entry.Medal);
            (*EntryObj)->TryGetStringField(TEXT("date"), Entry.Date);
            (*EntryObj)->TryGetStringField(TEXT("vehicle"), Entry.VehicleUsed);
            (*EntryObj)->TryGetNumberField(TEXT("collisions"), Entry.Collisions);
            (*EntryObj)->TryGetBoolField(TEXT("is_player"), Entry.bIsPlayer);
            Entries.Add(Entry);
        }
    }

    SortEntries(Entries);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Leaderboard loaded: %s (%d entries)"), *FullPath, Entries->Num());
    return true;
}

void ULeaderboardSystem::ClearLeaderboard(const FString& RouteId)
{
    Leaderboards.Remove(RouteId);
    FString FullPath = GetLeaderboardPath(RouteId);
    IFileManager::Get().Delete(*FullPath);
}

void ULeaderboardSystem::SeedDefaultEntries(const FString& RouteId, float GoldTime, float SilverTime, float BronzeTime)
{
    TArray<FLeaderboardEntry> Entries;

    auto MakeEntry = [](const FString& Name, float Time, const FString& Medal, const FString& Date) -> FLeaderboardEntry
    {
        FLeaderboardEntry Entry;
        Entry.PlayerName = Name;
        Entry.TimeSeconds = Time;
        Entry.Medal = Medal;
        Entry.Date = Date;
        Entry.VehicleUsed = TEXT("Sedan");
        Entry.Collisions = 0;
        Entry.bIsPlayer = false;
        return Entry;
    };

    Entries.Add(MakeEntry(TEXT("NitroKing"), GoldTime * 0.92f, TEXT("GOLD"), TEXT("2026-06-01")));
    Entries.Add(MakeEntry(TEXT("DriftQueen"), GoldTime * 0.97f, TEXT("GOLD"), TEXT("2026-06-02")));
    Entries.Add(MakeEntry(TEXT("SpeedDemon"), GoldTime, TEXT("GOLD"), TEXT("2026-06-03")));
    Entries.Add(MakeEntry(TEXT("RoadRunner"), (GoldTime + SilverTime) * 0.5f, TEXT("SILVER"), TEXT("2026-06-02")));
    Entries.Add(MakeEntry(TEXT("CruiseCtrl"), SilverTime, TEXT("SILVER"), TEXT("2026-06-03")));
    Entries.Add(MakeEntry(TEXT("SundayDrv"), (SilverTime + BronzeTime) * 0.5f, TEXT("BRONZE"), TEXT("2026-06-01")));
    Entries.Add(MakeEntry(TEXT("RookieOne"), BronzeTime, TEXT("BRONZE"), TEXT("2026-06-03")));

    Leaderboards.Add(RouteId, Entries);
    SaveLeaderboard(RouteId);
}

bool ULeaderboardSystem::HasLeaderboard(const FString& RouteId) const
{
    return Leaderboards.Contains(RouteId);
}

FString ULeaderboardSystem::GetLeaderboardPath(const FString& RouteId) const
{
    return FPaths::ProjectSavedDir() / TEXT("Leaderboards") /
        FString::Printf(TEXT("leaderboard_%s.json"), *RouteId);
}

void ULeaderboardSystem::SortEntries(TArray<FLeaderboardEntry>& Entries) const
{
    Entries.Sort([](const FLeaderboardEntry& A, const FLeaderboardEntry& B)
    {
        return A.TimeSeconds < B.TimeSeconds;
    });
}
