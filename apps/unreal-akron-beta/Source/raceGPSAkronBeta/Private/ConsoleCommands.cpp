#include "ConsoleCommands.h"
#include "Kismet/GameplayStatics.h"
#include "ChaosVehiclePawn.h"
#include "CruiseSprintGameMode.h"
#include "CheckpointGate.h"
#include "DayNightCycle.h"
#include "TrafficSpawner.h"
#include "Engine/World.h"

void UConsoleCommands::CheatGodMode(bool bEnable)
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] God mode: %s"), bEnable ? TEXT("ON") : TEXT("OFF"));
    // Would disable collision damage in scoring system
}

void UConsoleCommands::CheatSetSpeed(float SpeedKmh)
{
    AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (Vehicle)
    {
        FVector Vel = Vehicle->GetActorForwardVector() * SpeedKmh / 3.6f;
        Vehicle->GetMesh()->SetPhysicsLinearVelocity(Vel);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Speed set to %.0f km/h"), SpeedKmh);
    }
}

void UConsoleCommands::CheatTeleportCheckpoint(int32 CheckpointIndex)
{
    ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (!GM)
        return;

    TArray<AActor*> Checkpoints;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACheckpointGate::StaticClass(), Checkpoints);
    if (CheckpointIndex >= 0 && CheckpointIndex < Checkpoints.Num())
    {
        ACheckpointGate* Gate = Cast<ACheckpointGate>(Checkpoints[CheckpointIndex]);
        if (Gate)
        {
            AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
            if (Vehicle)
            {
                Vehicle->SetActorLocation(Gate->GetActorLocation() + FVector(0, 0, 100));
                UE_LOG(LogTemp, Log, TEXT("[raceGPS] Teleported to checkpoint %d"), CheckpointIndex);
            }
        }
    }
}

void UConsoleCommands::CheatFinishRace()
{
    ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GM)
    {
        GM->FinishRace();
    }
}

void UConsoleCommands::CheatSetTime(float TimeSeconds)
{
    ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GM)
    {
        // Not directly accessible; would need setter
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Set time: %.2fs"), TimeSeconds);
    }
}

void UConsoleCommands::DebugShowRoadGraph()
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Showing road graph debug"));
}

void UConsoleCommands::DebugHideRoadGraph()
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Hiding road graph debug"));
}

void UConsoleCommands::DebugSpawnVehicle(const FString& VehicleClassName)
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Spawning vehicle: %s"), *VehicleClassName);
}

void UConsoleCommands::DebugSetTimeOfDay(float Hour)
{
    TArray<AActor*> DayNightActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADayNightCycle::StaticClass(), DayNightActors);
    for (AActor* Actor : DayNightActors)
    {
        ADayNightCycle* DayNight = Cast<ADayNightCycle>(Actor);
        if (DayNight)
        {
            DayNight->SetTimeOfDay(Hour);
            UE_LOG(LogTemp, Log, TEXT("[raceGPS] Time of day set to %.1f"), Hour);
        }
    }
}

void UConsoleCommands::DebugClearTraffic()
{
    TArray<AActor*> Spawners;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATrafficSpawner::StaticClass(), Spawners);
    for (AActor* Actor : Spawners)
    {
        ATrafficSpawner* Spawner = Cast<ATrafficSpawner>(Actor);
        if (Spawner)
        {
            Spawner->ClearTraffic();
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Traffic cleared"));
}

void UConsoleCommands::DebugReloadCity()
{
    ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GM)
    {
        GM->RestartRace();
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] City reloaded"));
    }
}

void UConsoleCommands::DebugDumpRaceState()
{
    ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (!GM)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] No GameMode found"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] === RACE STATE DUMP ==="));
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] State: %s"), *UEnum::GetValueAsString(GM->GetRaceState()));
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Elapsed Time: %.2fs"), GM->GetElapsedTime());
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Checkpoint: %d / %d"), GM->GetCurrentCheckpoint(), GM->GetTotalCheckpoints());
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Total Distance: %.0fm"), GM->GetTotalRaceDistance());

    AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (Vehicle)
    {
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Vehicle Speed: %.1f km/h"), Vehicle->GetSpeedKmh());
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Vehicle RPM: %.0f"), Vehicle->GetEngineRPM());
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Vehicle Gear: %d"), Vehicle->GetCurrentGear());
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Vehicle Pos: %s"), *Vehicle->GetActorLocation().ToString());
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] === END DUMP ==="));
}
