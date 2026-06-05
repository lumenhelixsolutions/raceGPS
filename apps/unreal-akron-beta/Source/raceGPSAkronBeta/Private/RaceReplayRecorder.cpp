#include "RaceReplayRecorder.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void URaceReplayRecorder::StartRecording()
{
    bRecording = true;
    RecordingDuration = 0.0f;
    LastRecordTime = 0.0f;
    Frames.Empty();
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Replay recording started"));
}

void URaceReplayRecorder::StopRecording()
{
    bRecording = false;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Replay recording stopped. Frames: %d, Duration: %.2fs"),
        Frames.Num(), RecordingDuration);
}

void URaceReplayRecorder::RecordFrame(const FReplayFrame& Frame)
{
    if (!bRecording)
        return;

    if (Frames.Num() == 0 || Frame.Timestamp - LastRecordTime >= RecordInterval)
    {
        Frames.Add(Frame);
        LastRecordTime = Frame.Timestamp;
        RecordingDuration = Frame.Timestamp;
    }
}

void URaceReplayRecorder::ClearRecording()
{
    Frames.Empty();
    RecordingDuration = 0.0f;
    LastRecordTime = 0.0f;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Replay recording cleared"));
}

bool URaceReplayRecorder::SaveToFile(const FString& Filename) const
{
    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("version"), 1);
    Root->SetNumberField(TEXT("duration"), RecordingDuration);
    Root->SetNumberField(TEXT("frame_count"), Frames.Num());
    Root->SetNumberField(TEXT("record_interval"), RecordInterval);

    TArray<TSharedPtr<FJsonValue>> FrameArray;
    for (const FReplayFrame& Frame : Frames)
    {
        TSharedPtr<FJsonObject> FrameObj = MakeShared<FJsonObject>();
        FrameObj->SetNumberField(TEXT("t"), Frame.Timestamp);
        FrameObj->SetNumberField(TEXT("x"), Frame.Location.X);
        FrameObj->SetNumberField(TEXT("y"), Frame.Location.Y);
        FrameObj->SetNumberField(TEXT("z"), Frame.Location.Z);
        FrameObj->SetNumberField(TEXT("pitch"), Frame.Rotation.Pitch);
        FrameObj->SetNumberField(TEXT("yaw"), Frame.Rotation.Yaw);
        FrameObj->SetNumberField(TEXT("roll"), Frame.Rotation.Roll);
        FrameObj->SetNumberField(TEXT("speed"), Frame.SpeedKmh);
        FrameObj->SetNumberField(TEXT("throttle"), Frame.Throttle);
        FrameObj->SetNumberField(TEXT("steer"), Frame.Steering);
        FrameObj->SetNumberField(TEXT("brake"), Frame.Brake);
        FrameObj->SetBoolField(TEXT("handbrake"), Frame.bHandbrake);
        FrameArray.Add(MakeShared<FJsonValueObject>(FrameObj));
    }
    Root->SetArrayField(TEXT("frames"), FrameArray);

    FString Content;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    FString FullPath = FPaths::ProjectSavedDir() / TEXT("Replays") / Filename;
    FPaths::MakeStandardFilename(FullPath);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);

    bool bSuccess = FFileHelper::SaveStringToFile(Content, *FullPath);
    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Replay saved: %s (%d frames)"), *FullPath, Frames.Num());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to save replay: %s"), *FullPath);
    }
    return bSuccess;
}

bool URaceReplayRecorder::LoadFromFile(const FString& Filename)
{
    FString FullPath = FPaths::ProjectSavedDir() / TEXT("Replays") / Filename;
    FPaths::MakeStandardFilename(FullPath);

    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to load replay: %s"), *FullPath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    if (!FJsonSerializer::Deserialize(Reader, Root))
    {
        UE_LOG(LogTemp, Error, TEXT("[raceGPS] Failed to parse replay JSON"));
        return false;
    }

    Frames.Empty();
    RecordingDuration = 0.0f;

    const TArray<TSharedPtr<FJsonValue>>* FrameArray;
    if (Root->TryGetArrayField(TEXT("frames"), FrameArray))
    {
        Frames.Reserve(FrameArray->Num());
        for (const TSharedPtr<FJsonValue>& Val : *FrameArray)
        {
            const TSharedPtr<FJsonObject>* FrameObj;
            if (!Val->TryGetObject(FrameObj))
                continue;

            FReplayFrame Frame;
            (*FrameObj)->TryGetNumberField(TEXT("t"), Frame.Timestamp);
            double X = 0.0, Y = 0.0, Z = 0.0;
            (*FrameObj)->TryGetNumberField(TEXT("x"), X);
            (*FrameObj)->TryGetNumberField(TEXT("y"), Y);
            (*FrameObj)->TryGetNumberField(TEXT("z"), Z);
            Frame.Location = FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));

            double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
            (*FrameObj)->TryGetNumberField(TEXT("pitch"), Pitch);
            (*FrameObj)->TryGetNumberField(TEXT("yaw"), Yaw);
            (*FrameObj)->TryGetNumberField(TEXT("roll"), Roll);
            Frame.Rotation = FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw), static_cast<float>(Roll));

            (*FrameObj)->TryGetNumberField(TEXT("speed"), Frame.SpeedKmh);
            (*FrameObj)->TryGetNumberField(TEXT("throttle"), Frame.Throttle);
            (*FrameObj)->TryGetNumberField(TEXT("steer"), Frame.Steering);
            (*FrameObj)->TryGetNumberField(TEXT("brake"), Frame.Brake);
            (*FrameObj)->TryGetBoolField(TEXT("handbrake"), Frame.bHandbrake);

            Frames.Add(Frame);
            RecordingDuration = FMath::Max(RecordingDuration, Frame.Timestamp);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Replay loaded: %s (%d frames, %.2fs)"),
        *FullPath, Frames.Num(), RecordingDuration);
    return Frames.Num() > 0;
}
