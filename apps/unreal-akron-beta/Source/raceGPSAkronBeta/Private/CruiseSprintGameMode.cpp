#include "CruiseSprintGameMode.h"
#include "ChaosVehiclePawn.h"
#include "AkronXodrImporter.h"
#include "CheckpointGate.h"
#include "RouteSplineActor.h"
#include "RoadMeshGenerator.h"
#include "PauseMenuWidget.h"
#include "RaceScoringSystem.h"
#include "RaceReplayManager.h"
#include "MinimapWidget.h"
#include "CompassWidget.h"
#include "DeveloperConsole.h"
#include "GhostVehicle.h"
#include "DayNightCycle.h"
#include "TrafficSpawner.h"
#include "NeonHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/SplineComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerStart.h"

ACruiseSprintGameMode::ACruiseSprintGameMode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DefaultPawnClass = AChaosVehiclePawn::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
    ScoringSystem = CreateDefaultSubobject<URaceScoringSystem>(TEXT("ScoringSystem"));
}

void ACruiseSprintGameMode::StartPlay()
{
    Super::StartPlay();

    if (!ReplayManager)
    {
        ReplayManager = NewObject<URaceReplayManager>(this);
    }

    LoadCityData();
    CurrentState = ECruiseSprintState::Loading;

    // Spawn road meshes asynchronously
    FActorSpawnParameters RoadParams;
    RoadParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ARoadMeshGenerator* RoadGen = GetWorld()->SpawnActor<ARoadMeshGenerator>(
        ARoadMeshGenerator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, RoadParams);
    if (RoadGen)
    {
        RoadGen->XodrPath = CityPackPath + XodrFile;
        RoadGen->GenerateRoadMeshAsync();
    }

    // After road generation + brief load, transition to countdown
    FTimerHandle LoadTimer;
    GetWorld()->GetTimerManager().SetTimer(LoadTimer, [this]()
    {
        CurrentState = ECruiseSprintState::Countdown;
        CountdownTimer = CountdownDuration;
        OnRaceStateChanged(CurrentState);
    }, 3.0f, false);
}

void ACruiseSprintGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CurrentState == ECruiseSprintState::Countdown)
    {
        UpdateCountdown(DeltaTime);
    }
    else if (CurrentState == ECruiseSprintState::Racing)
    {
        ElapsedTime += DeltaTime;
        if (ReplayManager)
        {
            ReplayManager->TickRecording(DeltaTime);
        }
    }

    if (ReplayManager)
    {
        ReplayManager->TickPlayback(DeltaTime);
    }
}

void ACruiseSprintGameMode::InitHUDWidgets()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
        return;

    if (MinimapClass)
    {
        MinimapWidget = CreateWidget<UMinimapWidget>(PC, MinimapClass);
        if (MinimapWidget)
        {
            MinimapWidget->AddToViewport(10);
        }
    }

    if (CompassClass)
    {
        CompassWidget = CreateWidget<UCompassWidget>(PC, CompassClass);
        if (CompassWidget)
        {
            CompassWidget->AddToViewport(10);
        }
    }

    if (DeveloperConsoleClass)
    {
        DevConsole = CreateWidget<UDeveloperConsole>(PC, DeveloperConsoleClass);
        if (DevConsole)
        {
            DevConsole->AddToViewport(100);
        }
    }
}

void ACruiseSprintGameMode::OnVehicleCollision(float ImpactSpeedKmh)
{
    if (ScoringSystem && CurrentState == ECruiseSprintState::Racing)
    {
        ScoringSystem->OnCollision(ImpactSpeedKmh);
    }
}

void ACruiseSprintGameMode::LoadCityData()
{
    FString ManifestPath = CityPackPath + ManifestFile;
    UAkronXodrImporter::LoadManifest(ManifestPath, WorldOriginLat, WorldOriginLon);

    UAkronXodrImporter::LoadRouteSplines(RouteDir, LoadedRoutes);
    UAkronXodrImporter::LoadSpawnPoints(ManifestPath, LoadedSpawns);
    UAkronXodrImporter::LoadPOIs(ManifestPath, LoadedPOIs);

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] City data loaded. Routes: %d, Spawns: %d, POIs: %d"),
        LoadedRoutes.Num(), LoadedSpawns.Num(), LoadedPOIs.Num());
}

void ACruiseSprintGameMode::SpawnPlayerAtStart()
{
    if (LoadedSpawns.Num() == 0) return;

    FAkronSpawnPoint& Spawn = LoadedSpawns[0];
    FVector WorldLoc = UAkronXodrImporter::GeoToWorld(
        Spawn.Location.Z, Spawn.Location.X, WorldOriginLat, WorldOriginLon);
    WorldLoc.Z = 50.0f; // Slight lift off ground

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC && PC->GetPawn())
    {
        PC->GetPawn()->SetActorLocationAndRotation(WorldLoc, Spawn.Rotation, false, nullptr, ETeleportType::ResetPhysics);
    }
}

void ACruiseSprintGameMode::SpawnRouteSpline()
{
    if (LoadedRoutes.Num() == 0 || SelectedRouteIndex >= LoadedRoutes.Num()) return;

    const FAkronRouteSpline& Route = LoadedRoutes[SelectedRouteIndex];
    if (Route.Waypoints.Num() < 2) return;

    // Convert raw lat/lon waypoints to world space
    TArray<FVector> WorldWaypoints;
    for (const FVector& Wp : Route.Waypoints)
    {
        FVector WorldLoc = UAkronXodrImporter::GeoToWorld(
            -Wp.Z, Wp.X, WorldOriginLat, WorldOriginLon);
        WorldLoc.Z = 50.0f;
        WorldWaypoints.Add(WorldLoc);
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ARouteSplineActor* RouteActor = GetWorld()->SpawnActor<ARouteSplineActor>(
        ARouteSplineActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);

    if (RouteActor)
    {
        RouteActor->RouteId = Route.RouteId;
        RouteActor->BuildSplineFromWaypoints(WorldWaypoints);
    }

    // Spawn ghost car
    AGhostVehicle* Ghost = GetWorld()->SpawnActor<AGhostVehicle>(
        AGhostVehicle::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (Ghost)
    {
        Ghost->SetRouteWaypoints(WorldWaypoints);
        Ghost->StartGhostRun(CountdownDuration + 2.0f);
    }
}

void ACruiseSprintGameMode::SpawnCheckpoints()
{
    if (LoadedRoutes.Num() == 0 || SelectedRouteIndex >= LoadedRoutes.Num()) return;

    const FAkronRouteSpline& Route = LoadedRoutes[SelectedRouteIndex];
    for (int32 i = 0; i < Route.CheckpointLocations.Num(); ++i)
    {
        FVector WorldLoc = UAkronXodrImporter::GeoToWorld(
            -Route.CheckpointLocations[i].Z, Route.CheckpointLocations[i].X, WorldOriginLat, WorldOriginLon);
        WorldLoc.Z = 100.0f;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ACheckpointGate* Gate = GetWorld()->SpawnActor<ACheckpointGate>(ACheckpointGate::StaticClass(), WorldLoc, FRotator::ZeroRotator, Params);

        if (Gate)
        {
            Gate->CheckpointIndex = i;
            Gate->ActivateGate();
            // Bind delegate to route checkpoint reached
            Gate->OnCheckpointReached.AddDynamic(this, &ACruiseSprintGameMode::OnCheckpointReached);
        }
    }
}

void ACruiseSprintGameMode::UpdateCountdown(float DeltaTime)
{
    CountdownTimer -= DeltaTime;
    if (CountdownTimer <= 0.0f)
    {
        CurrentState = ECruiseSprintState::Racing;
        ElapsedTime = 0.0f;
        CurrentCheckpoint = 0;
        SpawnPlayerAtStart();
        SpawnCheckpoints();
        OnRaceStateChanged(CurrentState);
    }
}

void ACruiseSprintGameMode::StartRace()
{
    CurrentState = ECruiseSprintState::Countdown;
    CountdownTimer = CountdownDuration;
    ElapsedTime = 0.0f;
    CurrentCheckpoint = 0;
    SpawnRouteSpline();

    // Load best replay ghost
    if (ReplayManager && LoadedRoutes.Num() > 0 && SelectedRouteIndex < LoadedRoutes.Num())
    {
        FString RouteId = LoadedRoutes[SelectedRouteIndex].RouteId;
        if (ReplayManager->HasBestReplay(RouteId))
        {
            ReplayManager->LoadBestReplay(RouteId);
            if (BestGhost)
            {
                ReplayManager->PlayBestReplay(BestGhost, CountdownDuration + 2.0f);
            }
        }
    }

    OnRaceStateChanged(CurrentState);
}

void ACruiseSprintGameMode::PauseRace()
{
    if (CurrentState == ECruiseSprintState::Racing)
    {
        CurrentState = ECruiseSprintState::Paused;
        UGameplayStatics::SetGamePaused(GetWorld(), true);
        OnRaceStateChanged(CurrentState);

        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC && PauseMenuClass)
        {
            ActivePauseMenu = CreateWidget<UPauseMenuWidget>(PC, PauseMenuClass);
            if (ActivePauseMenu)
            {
                ActivePauseMenu->AddToViewport(100);
                PC->SetInputMode(FInputModeUIOnly());
                PC->bShowMouseCursor = true;
            }
        }
    }
}

void ACruiseSprintGameMode::ResumeRace()
{
    if (CurrentState == ECruiseSprintState::Paused)
    {
        CurrentState = ECruiseSprintState::Racing;
        UGameplayStatics::SetGamePaused(GetWorld(), false);
        OnRaceStateChanged(CurrentState);

        if (ActivePauseMenu)
        {
            ActivePauseMenu->RemoveFromParent();
            ActivePauseMenu = nullptr;
        }

        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC)
        {
            PC->SetInputMode(FInputModeGameOnly());
            PC->bShowMouseCursor = false;
        }
    }
}

void ACruiseSprintGameMode::FinishRace()
{
    CurrentState = ECruiseSprintState::Finished;

    if (ScoringSystem)
    {
        FRaceScore Score = ScoringSystem->CalculateFinalScore(ElapsedTime);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race finished! Base: %.2fs, Penalties: %.2fs, Bonus: %.2fs, Final: %.2fs, Medal: %s"),
            Score.BaseTime, Score.CollisionPenalty + Score.MissedCheckpointPenalty, Score.CleanDrivingBonus,
            Score.FinalTime, *Score.Medal);

        UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());
        if (GI && LoadedRoutes.Num() > 0 && SelectedRouteIndex < LoadedRoutes.Num())
        {
            GI->UpdateBestTime(LoadedRoutes[SelectedRouteIndex].RouteId, Score.FinalTime);
        }
    }

    // Save replay if it's the best
    if (ReplayManager && LoadedRoutes.Num() > 0 && SelectedRouteIndex < LoadedRoutes.Num())
    {
        ReplayManager->EndRaceRecording();
        FString RouteId = LoadedRoutes[SelectedRouteIndex].RouteId;

        UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());
        if (GI)
        {
            float BestTime = GI->GetBestTime(RouteId);
            if (BestTime < 0.0f || ElapsedTime <= BestTime)
            {
                ReplayManager->SaveBestReplay(RouteId);
                UE_LOG(LogTemp, Log, TEXT("[raceGPS] New best replay saved for %s"), *RouteId);
            }
        }
    }

    OnRaceStateChanged(CurrentState);
}

void ACruiseSprintGameMode::RestartRace()
{
    CurrentState = ECruiseSprintState::Countdown;
    CountdownTimer = CountdownDuration;
    ElapsedTime = 0.0f;
    CurrentCheckpoint = 0;
    if (ScoringSystem)
    {
        ScoringSystem->Reset();
    }
    if (ReplayManager)
    {
        ReplayManager->BeginRaceRecording();
    }
    OnRaceStateChanged(CurrentState);
}

void ACruiseSprintGameMode::OnCheckpointReached(int32 CheckpointIndex)
{
    if (CurrentState != ECruiseSprintState::Racing) return;
    if (CheckpointIndex == CurrentCheckpoint)
    {
        CurrentCheckpoint++;
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Checkpoint %d reached"), CheckpointIndex);

        if (CurrentCheckpoint >= GetTotalCheckpoints())
        {
            FinishRace();
        }
    }
}

void ACruiseSprintGameMode::OnCheckpointReached()
{
    // Overload for delegate binding — delegates don't support int32 params directly
    // The actual index is handled by the lambda/caller binding
}

int32 ACruiseSprintGameMode::GetTotalCheckpoints() const
{
    if (LoadedRoutes.Num() == 0 || SelectedRouteIndex >= LoadedRoutes.Num()) return 0;
    return LoadedRoutes[SelectedRouteIndex].CheckpointLocations.Num();
}

float ACruiseSprintGameMode::GetTotalRaceDistance() const
{
    if (LoadedRoutes.Num() == 0 || SelectedRouteIndex >= LoadedRoutes.Num()) return 0.0f;
    return LoadedRoutes[SelectedRouteIndex].TotalDistanceMeters;
}

void ACruiseSprintGameMode::OnRaceStateChanged(ECruiseSprintState NewState)
{
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race state changed to: %s"),
        *UEnum::GetValueAsString(NewState));
}
