#include "RaceReplayPlayer.h"
#include "RaceReplayRecorder.h"
#include "GhostVehicle.h"

void URaceReplayPlayer::SetRecorder(URaceReplayRecorder* InRecorder)
{
    Recorder = InRecorder;
    CurrentFrameIndex = 0;
    CurrentTime = 0.0f;
}

void URaceReplayPlayer::SetGhostActor(AGhostVehicle* InGhost)
{
    GhostActor = InGhost;
}

void URaceReplayPlayer::StartPlayback(float DelaySeconds)
{
    if (!Recorder || Recorder->Frames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] No replay data to play"));
        return;
    }

    CurrentTime = 0.0f;
    CurrentFrameIndex = 0;

    FTimerHandle DelayTimer;
    GetWorld()->GetTimerManager().SetTimer(DelayTimer, [this]()
    {
        bPlaying = true;
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Replay playback started. Duration: %.2fs"), GetTotalDuration());
    }, DelaySeconds, false);
}

void URaceReplayPlayer::StopPlayback()
{
    bPlaying = false;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Replay playback stopped at %.2fs"), CurrentTime);
}

void URaceReplayPlayer::SeekToTime(float TimeSeconds)
{
    if (!Recorder || Recorder->Frames.Num() == 0)
        return;

    CurrentTime = FMath::Clamp(TimeSeconds, 0.0f, GetTotalDuration());

    // Binary search for closest frame
    int32 Low = 0;
    int32 High = Recorder->Frames.Num() - 1;
    while (Low < High)
    {
        int32 Mid = (Low + High) / 2;
        if (Recorder->Frames[Mid].Timestamp < CurrentTime)
        {
            Low = Mid + 1;
        }
        else
        {
            High = Mid;
        }
    }
    CurrentFrameIndex = Low;

    // Apply frame
    if (GhostActor && CurrentFrameIndex < Recorder->Frames.Num())
    {
        const FReplayFrame& Frame = Recorder->Frames[CurrentFrameIndex];
        GhostActor->SetActorLocationAndRotation(Frame.Location, Frame.Rotation);
    }
}

void URaceReplayPlayer::TickPlayback(float DeltaTime)
{
    if (!bPlaying || !Recorder || Recorder->Frames.Num() == 0)
        return;

    CurrentTime += DeltaTime * PlaybackSpeed;

    if (CurrentTime >= GetTotalDuration())
    {
        bPlaying = false;
        CurrentTime = GetTotalDuration();
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Replay playback complete"));
        return;
    }

    // Advance frame index
    while (CurrentFrameIndex < Recorder->Frames.Num() - 1 &&
           Recorder->Frames[CurrentFrameIndex + 1].Timestamp <= CurrentTime)
    {
        CurrentFrameIndex++;
    }

    // Interpolate between frames
    if (CurrentFrameIndex < Recorder->Frames.Num() - 1 && GhostActor)
    {
        const FReplayFrame& FrameA = Recorder->Frames[CurrentFrameIndex];
        const FReplayFrame& FrameB = Recorder->Frames[CurrentFrameIndex + 1];

        float Alpha = 0.0f;
        if (FrameB.Timestamp > FrameA.Timestamp)
        {
            Alpha = (CurrentTime - FrameA.Timestamp) / (FrameB.Timestamp - FrameA.Timestamp);
        }

        FVector Location = FMath::Lerp(FrameA.Location, FrameB.Location, Alpha);
        FRotator Rotation = FMath::Lerp(FrameA.Rotation, FrameB.Rotation, Alpha);
        GhostActor->SetActorLocationAndRotation(Location, Rotation);
    }
    else if (CurrentFrameIndex < Recorder->Frames.Num() && GhostActor)
    {
        const FReplayFrame& Frame = Recorder->Frames[CurrentFrameIndex];
        GhostActor->SetActorLocationAndRotation(Frame.Location, Frame.Rotation);
    }
}

float URaceReplayPlayer::GetTotalDuration() const
{
    if (!Recorder || Recorder->Frames.Num() == 0)
        return 0.0f;
    return Recorder->Frames.Last().Timestamp;
}
