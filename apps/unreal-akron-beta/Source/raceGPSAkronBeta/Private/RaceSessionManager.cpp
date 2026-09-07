// Copyright raceGPS. All Rights Reserved.

#include "RaceSessionManager.h"
#include "PlayerProfileSaveGame.h"
#include "RaceTimerSystem.h"
#include "DriftScoreSystem.h"
#include "GhostVehicleSystem.h"
#include "ChaosVehiclePawn.h"
#include "VehicleTuningData.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

URaceSessionManager* URaceSessionManager::GetInstance(UObject* WorldContextObject)
{
    UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
    if (!World && GEngine)
    {
        World = GEngine->GetCurrentPlayWorld();
    }
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] RaceSessionManager::GetInstance failed: no world"));
        return nullptr;
    }

    UGameInstance* GI = World->GetGameInstance();
    if (!GI)
    {
        return nullptr;
    }

    URaceSessionManager* Existing = Cast<URaceSessionManager>(
        StaticFindObject(URaceSessionManager::StaticClass(), GI, TEXT("RaceSessionManager")));
    if (Existing)
    {
        return Existing;
    }

    URaceSessionManager* NewManager = NewObject<URaceSessionManager>(
        GI, URaceSessionManager::StaticClass(), TEXT("RaceSessionManager"));
    return NewManager;
}

void URaceSessionManager::StartSession(const FString& TrackId, const FString& CarBuildId)
{
    CurrentTrackId = TrackId;
    CurrentCarBuildId = CarBuildId;
    CurrentCheckpoint = 0;
    TotalCheckpoints = 0;
    ElapsedTime = 0.0f;
    CountdownTimer = 0.0f;
    DriftScoreAtFinish = 0.0f;

    ActiveProfile = UPlayerProfileSaveGame::LoadProfile();
    if (!ActiveProfile)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] RaceSessionManager: failed to load profile; using transient defaults"));
    }

    InitializeSystems();
    SetState(ERaceSessionState::Menu);
}

void URaceSessionManager::InitializeSystems()
{
    if (!RaceTimer)
    {
        RaceTimer = NewObject<URaceTimerSystem>(this);
    }
    if (!DriftSystem)
    {
        DriftSystem = NewObject<UDriftScoreSystem>(this);
        DriftSystem->ResetScore();
    }
    if (!GhostSystem)
    {
        GhostSystem = NewObject<UGhostVehicleSystem>(this);
    }
}

void URaceSessionManager::SetVehicleTuningData(UVehicleTuningData* TuningData)
{
    SelectedVehicleTuning = TuningData;
}

void URaceSessionManager::StartRace()
{
    if (CurrentState != ERaceSessionState::Menu && CurrentState != ERaceSessionState::Finished)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Cannot StartRace from state %s"), *UEnum::GetValueAsString(CurrentState));
        return;
    }

    CurrentCheckpoint = 0;
    ElapsedTime = 0.0f;
    CountdownTimer = CountdownDuration;
    DriftScoreAtFinish = 0.0f;

    if (RaceTimer)
    {
        RaceTimer->StartRace();
    }
    if (DriftSystem)
    {
        DriftSystem->ResetScore();
    }
    if (GhostSystem)
    {
        GhostSystem->StartRecording();
    }

    SpawnPlayerVehicle();
    SetState(ERaceSessionState::Countdown);
}

void URaceSessionManager::SpawnPlayerVehicle()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
        return;

    AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(PC->GetPawn());
    if (Vehicle && SelectedVehicleTuning)
    {
        Vehicle->SetTuningData(SelectedVehicleTuning);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Applied tuning data to player vehicle"));
    }
}

void URaceSessionManager::PauseRace()
{
    if (CurrentState == ERaceSessionState::Racing)
    {
        SetState(ERaceSessionState::Paused);
        UGameplayStatics::SetGamePaused(GetWorld(), true);
    }
}

void URaceSessionManager::ResumeRace()
{
    if (CurrentState == ERaceSessionState::Paused)
    {
        SetState(ERaceSessionState::Racing);
        UGameplayStatics::SetGamePaused(GetWorld(), false);
    }
}

void URaceSessionManager::EndRace()
{
    if (CurrentState != ERaceSessionState::Racing && CurrentState != ERaceSessionState::Countdown)
        return;

    if (RaceTimer)
    {
        RaceTimer->EndRace();
    }
    if (GhostSystem)
    {
        GhostSystem->StopRecording();
    }

    FRaceResults Results;
    Results.TrackId = CurrentTrackId;
    Results.FinalTime = ElapsedTime;
    Results.BestLapTime = RaceTimer ? RaceTimer->GetBestLapTime() : 0.0f;
    Results.TotalDriftScore = DriftSystem ? DriftSystem->GetCurrentScore() : 0.0f;
    Results.CurrentCheckpoint = CurrentCheckpoint;
    Results.TotalCheckpoints = TotalCheckpoints;
    Results.Medal = CalculateMedal(ElapsedTime);

    CalculateRewards(Results);
    ShowEndScreen(Results);
    SetState(ERaceSessionState::Finished);
}

void URaceSessionManager::TickSession(float DeltaTime)
{
    if (CurrentState == ERaceSessionState::Countdown)
    {
        UpdateCountdown(DeltaTime);
    }
    else if (CurrentState == ERaceSessionState::Racing)
    {
        ElapsedTime += DeltaTime;
        if (RaceTimer && !RaceTimer->IsLapActive())
        {
            RaceTimer->StartLap();
        }
    }
}

void URaceSessionManager::UpdateCountdown(float DeltaTime)
{
    CountdownTimer -= DeltaTime;
    if (CountdownTimer <= 0.0f)
    {
        SetState(ERaceSessionState::Racing);
        CountdownTimer = 0.0f;
        if (RaceTimer)
        {
            RaceTimer->StartRace();
        }
    }
}

void URaceSessionManager::OnCheckpointReached(int32 CheckpointIndex)
{
    if (CurrentState != ERaceSessionState::Racing)
        return;

    if (CheckpointIndex == CurrentCheckpoint)
    {
        CurrentCheckpoint++;
        OnCheckpointPassed.Broadcast(CurrentCheckpoint);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Checkpoint %d reached"), CheckpointIndex);

        if (TotalCheckpoints > 0 && CurrentCheckpoint >= TotalCheckpoints)
        {
            EndRace();
        }
    }
}

void URaceSessionManager::SetState(ERaceSessionState NewState)
{
    if (NewState == CurrentState)
        return;

    CurrentState = NewState;
    OnStateChanged.Broadcast(CurrentState);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race state changed to: %s"), *UEnum::GetValueAsString(CurrentState));
}

FString URaceSessionManager::CalculateMedal(float TimeSeconds) const
{
    if (TimeSeconds <= 0.0f)
        return TEXT("NONE");
    if (TimeSeconds <= GoldTimeSeconds)
        return TEXT("GOLD");
    if (TimeSeconds <= SilverTimeSeconds)
        return TEXT("SILVER");
    if (TimeSeconds <= BronzeTimeSeconds)
        return TEXT("BRONZE");
    return TEXT("NONE");
}

void URaceSessionManager::CalculateRewards(FRaceResults& OutResults)
{
    int32 XP = 100; // Base finish XP

    if (OutResults.Medal == TEXT("GOLD"))
    {
        XP += 50;
    }
    else if (OutResults.Medal == TEXT("SILVER"))
    {
        XP += 25;
    }
    else if (OutResults.Medal == TEXT("BRONZE"))
    {
        XP += 10;
    }

    if (OutResults.FinalTime < GoldTimeSeconds)
    {
        XP += FMath::Clamp<int32>(static_cast<int32>((GoldTimeSeconds - OutResults.FinalTime) * 2.0f), 0, 200);
    }

    if (DriftSystem)
    {
        XP += FMath::Clamp<int32>(static_cast<int32>(DriftSystem->GetCurrentScore() / 10.0f), 0, 100);
    }

    OutResults.EarnedXP = XP;

    if (ActiveProfile)
    {
        ActiveProfile->RecordRaceFinished(true, OutResults.BestLapTime, CurrentTrackId);
        ActiveProfile->AddXP(XP);

        // Distance approximation: average speed 25 m/s * time
        float ApproxDistance = 25.0f * OutResults.FinalTime;
        ActiveProfile->AddDistance(ApproxDistance);
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Rewards: Medal=%s, XP=%d, Time=%.2f"),
        *OutResults.Medal, OutResults.EarnedXP, OutResults.FinalTime);
}

void URaceSessionManager::ShowEndScreen(const FRaceResults& Results)
{
    OnRaceFinished.Broadcast(Results);
}
