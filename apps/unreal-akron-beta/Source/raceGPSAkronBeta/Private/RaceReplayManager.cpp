#include "RaceReplayManager.h"
#include "RaceReplayRecorder.h"
#include "RaceReplayPlayer.h"
#include "ChaosVehiclePawn.h"
#include "GhostVehicle.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

URaceReplayManager::URaceReplayManager()
{
    Recorder = CreateDefaultSubobject<URaceReplayRecorder>(TEXT("Recorder"));
    Player = CreateDefaultSubobject<URaceReplayPlayer>(TEXT("Player"));
}

void URaceReplayManager::BeginRaceRecording()
{
    TargetVehicle = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!TargetVehicle)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] No player vehicle to record"));
        return;
    }

    Recorder->ClearRecording();
    Recorder->StartRecording();
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race recording started"));
}

void URaceReplayManager::EndRaceRecording()
{
    Recorder->StopRecording();
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race recording ended. %d frames"), Recorder->GetFrameCount());
}

void URaceReplayManager::TickRecording(float DeltaTime)
{
    if (!Recorder->IsRecording() || !TargetVehicle)
        return;

    FReplayFrame Frame;
    Frame.Timestamp = Recorder->GetRecordingDuration() + DeltaTime;
    Frame.Location = TargetVehicle->GetActorLocation();
    Frame.Rotation = TargetVehicle->GetActorRotation();
    Frame.SpeedKmh = TargetVehicle->GetSpeedKmh();

    // Input state not directly accessible from pawn; would need to expose
    // For now, zero inputs are recorded (position-only replay)
    Frame.Throttle = 0.0f;
    Frame.Steering = 0.0f;
    Frame.Brake = 0.0f;
    Frame.bHandbrake = false;

    Recorder->RecordFrame(Frame);
}

void URaceReplayManager::TickPlayback(float DeltaTime)
{
    if (Player->IsPlaying())
    {
        Player->TickPlayback(DeltaTime);
    }
}

void URaceReplayManager::PlayBestReplay(AGhostVehicle* GhostActor, float DelaySeconds)
{
    if (!GhostActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] No ghost actor for replay"));
        return;
    }

    Player->SetRecorder(Recorder);
    Player->SetGhostActor(GhostActor);
    Player->StartPlayback(DelaySeconds);
}

void URaceReplayManager::StopReplay()
{
    Player->StopPlayback();
}

bool URaceReplayManager::SaveBestReplay(const FString& RouteId)
{
    FString Filename = FString::Printf(TEXT("replay_%s.json"), *RouteId);
    return Recorder->SaveToFile(Filename);
}

bool URaceReplayManager::LoadBestReplay(const FString& RouteId)
{
    FString Filename = FString::Printf(TEXT("replay_%s.json"), *RouteId);
    return Recorder->LoadFromFile(Filename);
}

bool URaceReplayManager::HasBestReplay(const FString& RouteId) const
{
    FString FullPath = FPaths::ProjectSavedDir() / TEXT("Replays") /
        FString::Printf(TEXT("replay_%s.json"), *RouteId);
    FPaths::MakeStandardFilename(FullPath);
    return FPaths::FileExists(FullPath);
}

FString URaceReplayManager::GetReplayPath(const FString& RouteId) const
{
    return FPaths::ProjectSavedDir() / TEXT("Replays") /
        FString::Printf(TEXT("replay_%s.json"), *RouteId);
}
