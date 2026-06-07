// Copyright raceGPS. All Rights Reserved.

#include "ChampionshipManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonObjectConverter.h"

AChampionshipManager::AChampionshipManager()
{
    PrimaryActorTick.bCanEverTick = false;
    SaveDirectory = FPaths::ProjectSavedDir() / TEXT("Championships");
}

void AChampionshipManager::BeginPlay()
{
    Super::BeginPlay();
    if (Events.Num() == 0)
    {
        InitializeDefaultChampionship();
    }
}

void AChampionshipManager::InitializeDefaultChampionship()
{
    Events.Empty();
    Events.Add({TEXT("Downtown Dash"), TEXT("route_001"), TEXT("clear"), 25, 18, 15});
    Events.Add({TEXT("Harbor Run"), TEXT("route_002"), TEXT("rain"), 25, 18, 15});
    Events.Add({TEXT("Mountain Pass"), TEXT("route_003"), TEXT("fog"), 25, 18, 15});
    Events.Add({TEXT("Coastal Sprint"), TEXT("route_004"), TEXT("cloudy"), 25, 18, 15});
    Events.Add({TEXT("Night Storm"), TEXT("route_005"), TEXT("storm"), 25, 18, 15});
    CurrentEventIndex = 0;
    Standings.Empty();
    UE_LOG(LogTemp, Log, TEXT("Initialized championship: %s with %d events"), *ChampionshipName, Events.Num());
}

void AChampionshipManager::RecordRaceResult(const FString& DriverName, int32 Position, float BestLapTime)
{
    if (!Events.IsValidIndex(CurrentEventIndex)) return;

    const FChampionshipEvent& Event = Events[CurrentEventIndex];
    int32 Points = 0;
    bool bWin = false;
    bool bPodium = false;

    if (Position == 1) { Points = Event.PointsFirst; bWin = true; bPodium = true; }
    else if (Position == 2) { Points = Event.PointsSecond; bPodium = true; }
    else if (Position == 3) { Points = Event.PointsThird; bPodium = true; }

    FChampionshipStanding* Standing = Standings.FindByPredicate([&](const FChampionshipStanding& S) {
        return S.DriverName == DriverName;
    });

    if (Standing)
    {
        Standing->TotalPoints += Points;
        if (bWin) Standing->Wins++;
        if (bPodium) Standing->Podiums++;
    }
    else
    {
        FChampionshipStanding NewStanding;
        NewStanding.DriverName = DriverName;
        NewStanding.TotalPoints = Points;
        NewStanding.Wins = bWin ? 1 : 0;
        NewStanding.Podiums = bPodium ? 1 : 0;
        Standings.Add(NewStanding);
    }

    // Sort by points descending
    Standings.Sort([](const FChampionshipStanding& A, const FChampionshipStanding& B) {
        return A.TotalPoints > B.TotalPoints;
    });

    UE_LOG(LogTemp, Log, TEXT("Recorded result: %s P%d +%d pts"), *DriverName, Position, Points);
}

void AChampionshipManager::AdvanceToNextEvent()
{
    CurrentEventIndex++;
    if (CurrentEventIndex >= Events.Num())
    {
        UE_LOG(LogTemp, Log, TEXT("Championship complete!"));
    }
}

bool AChampionshipManager::IsChampionshipComplete() const
{
    return CurrentEventIndex >= Events.Num();
}

void AChampionshipManager::SaveChampionship(const FString& SlotName)
{
    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("championship_name"), ChampionshipName);
    Writer->WriteValue(TEXT("current_event_index"), CurrentEventIndex);
    Writer->WriteArrayStart(TEXT("standings"));
    for (const auto& S : Standings)
    {
        Writer->WriteObjectStart();
        Writer->WriteValue(TEXT("driver"), S.DriverName);
        Writer->WriteValue(TEXT("points"), S.TotalPoints);
        Writer->WriteValue(TEXT("wins"), S.Wins);
        Writer->WriteValue(TEXT("podiums"), S.Podiums);
        Writer->WriteObjectEnd();
    }
    Writer->WriteArrayEnd();
    Writer->WriteObjectEnd();
    Writer->Close();

    FString Path = SaveDirectory / SlotName + TEXT(".json");
    FFileHelper::SaveStringToFile(JsonString, *Path);
    UE_LOG(LogTemp, Log, TEXT("Championship saved: %s"), *Path);
}

void AChampionshipManager::LoadChampionship(const FString& SlotName)
{
    FString Path = SaveDirectory / SlotName + TEXT(".json");
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *Path))
    {
        UE_LOG(LogTemp, Warning, TEXT("No save found: %s"), *Path);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (FJsonSerializer::Deserialize(Reader, Root))
    {
        ChampionshipName = Root->GetStringField(TEXT("championship_name"));
        CurrentEventIndex = Root->GetIntegerField(TEXT("current_event_index"));
        Standings.Empty();
        const TArray<TSharedPtr<FJsonValue>>* Arr;
        if (Root->TryGetArrayField(TEXT("standings"), Arr))
        {
            for (const auto& Val : *Arr)
            {
                TSharedPtr<FJsonObject> Obj = Val->AsObject();
                if (Obj.IsValid())
                {
                    FChampionshipStanding S;
                    S.DriverName = Obj->GetStringField(TEXT("driver"));
                    S.TotalPoints = Obj->GetIntegerField(TEXT("points"));
                    S.Wins = Obj->GetIntegerField(TEXT("wins"));
                    S.Podiums = Obj->GetIntegerField(TEXT("podiums"));
                    Standings.Add(S);
                }
            }
        }
    }
    UE_LOG(LogTemp, Log, TEXT("Championship loaded: %s"), *Path);
}
