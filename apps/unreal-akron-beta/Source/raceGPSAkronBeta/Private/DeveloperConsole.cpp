#include "DeveloperConsole.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "ChaosVehiclePawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

void UDeveloperConsole::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDeveloperConsole::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (GetVisibility() == ESlateVisibility::Collapsed)
        return;

    AccumulatedTime += InDeltaTime;
    FrameCount++;

    if (AccumulatedTime >= UpdateInterval)
    {
        UpdateMetrics(AccumulatedTime / FrameCount);
        AccumulatedTime = 0.0f;
        FrameCount = 0;
    }
}

void UDeveloperConsole::ToggleVisibility()
{
    if (GetVisibility() == ESlateVisibility::Collapsed)
    {
        SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDeveloperConsole::UpdateMetrics(float DeltaTime)
{
    AvgFPS = 1.0f / DeltaTime;
    AvgFrameTime = DeltaTime * 1000.0f;

    if (FPSText)
    {
        FPSText->SetText(FText::FromString(FString::Printf(TEXT("FPS: %.1f"), AvgFPS)));
    }
    if (FrameTimeText)
    {
        FrameTimeText->SetText(FText::FromString(FString::Printf(TEXT("Frame: %.2f ms"), AvgFrameTime)));
    }
    if (PingText)
    {
        // No network for single-player beta, show local simulation time
        PingText->SetText(FText::FromString(FString::Printf(TEXT("Sim: %.2f ms"), AvgFrameTime)));
    }
    if (MemoryText)
    {
        FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
        MemoryText->SetText(FText::FromString(FString::Printf(TEXT("RAM: %s / %s"),
            *FormatBytes(Stats.UsedPhysical), *FormatBytes(Stats.AvailablePhysical))));
    }
    if (PositionText)
    {
        AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
        if (Vehicle)
        {
            FVector Loc = Vehicle->GetActorLocation();
            PositionText->SetText(FText::FromString(FString::Printf(TEXT("Pos: %.1f, %.1f, %.1f"), Loc.X, Loc.Y, Loc.Z)));
        }
    }
    if (VelocityText)
    {
        AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
        if (Vehicle)
        {
            FVector Vel = Vehicle->GetVelocity();
            float SpeedKmh = Vel.Size() * 0.036f;
            VelocityText->SetText(FText::FromString(FString::Printf(TEXT("Vel: %.1f km/h"), SpeedKmh)));
        }
    }
    if (RoadCountText)
    {
        RoadCountText->SetText(FText::FromString(TEXT("Roads: 1370")));
    }
}

FString UDeveloperConsole::FormatBytes(int64 Bytes) const
{
    if (Bytes < 1024)
        return FString::Printf(TEXT("%d B"), Bytes);
    if (Bytes < 1024 * 1024)
        return FString::Printf(TEXT("%.1f KB"), Bytes / 1024.0f);
    if (Bytes < 1024 * 1024 * 1024)
        return FString::Printf(TEXT("%.1f MB"), Bytes / (1024.0f * 1024.0f));
    return FString::Printf(TEXT("%.1f GB"), Bytes / (1024.0f * 1024.0f * 1024.0f));
}
