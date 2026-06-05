#include "CruiseSprintGameMode.h"
#include "ChaosVehiclePawn.h"
#include "AkronXodrImporter.h"
#include "CheckpointGate.h"
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
}

void ACruiseSprintGameMode::StartPlay()
{
    Super::StartPlay();
    LoadCityData();
    CurrentState = ECruiseSprintState::Loading;

    // After a brief load, transition to countdown
    FTimerHandle LoadTimer;
    GetWorld()->GetTimerManager().SetTimer(LoadTimer, [this]()
    {
        CurrentState = ECruiseSprintState::Countdown;
        CountdownTimer = CountdownDuration;
        OnRaceStateChanged(CurrentState);
    }, 1.0f, false);
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
    OnRaceStateChanged(CurrentState);
}

void ACruiseSprintGameMode::PauseRace()
{
    if (CurrentState == ECruiseSprintState::Racing)
    {
        CurrentState = ECruiseSprintState::Paused;
        UGameplayStatics::SetGamePaused(GetWorld(), true);
        OnRaceStateChanged(CurrentState);
    }
}

void ACruiseSprintGameMode::ResumeRace()
{
    if (CurrentState == ECruiseSprintState::Paused)
    {
        CurrentState = ECruiseSprintState::Racing;
        UGameplayStatics::SetGamePaused(GetWorld(), false);
        OnRaceStateChanged(CurrentState);
    }
}

void ACruiseSprintGameMode::FinishRace()
{
    CurrentState = ECruiseSprintState::Finished;
    OnRaceStateChanged(CurrentState);
}

void ACruiseSprintGameMode::RestartRace()
{
    CurrentState = ECruiseSprintState::Countdown;
    CountdownTimer = CountdownDuration;
    ElapsedTime = 0.0f;
    CurrentCheckpoint = 0;
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
