#include "CruiseSprintGameMode.h"
#include "raceGPSGameInstance.h"
#include "ChaosVehiclePawn.h"
#include "VehicleTuningData.h"
#include "AkronXodrImporter.h"
#include "BuildingMeshGenerator.h"
#include "StreetFurnitureSpawner.h"
#include "Version.h"
#include "CheckpointGate.h"
#include "RouteSplineActor.h"
#include "RoadMeshGenerator.h"
#include "PauseMenuWidget.h"
#include "RaceScoringSystem.h"
#include "RaceReplayManager.h"
#include "LeaderboardSystem.h"
#include "LoadingScreenWidget.h"
#include "PostRaceStatsWidget.h"
#include "TutorialSystem.h"
#include "TutorialWidget.h"
#include "AchievementSystem.h"
#include "ConsoleCommands.h"
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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
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

    // Resolve the active city (config / cvar / command line; defaults to Akron) and
    // let it override the Akron-flavored defaults of the path properties below.
    if (UAkronXodrImporter::ResolveCityLayout(CityLayout))
    {
        CityPackPath = CityLayout.CitypackDir + TEXT("/");
        if (!CityLayout.ManifestPath.IsEmpty())
        {
            ManifestFile = FPaths::GetCleanFilename(CityLayout.ManifestPath);
        }
        if (!CityLayout.XodrPath.IsEmpty())
        {
            XodrFile = FPaths::GetCleanFilename(CityLayout.XodrPath);
        }
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Active city: %s (%s)"), *CityLayout.CityId, *CityLayout.DisplayName);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Could not resolve city layout for '%s'; falling back to Akron defaults"),
            *UAkronXodrImporter::GetActiveCityId());
    }

    if (!ReplayManager)
    {
        ReplayManager = NewObject<URaceReplayManager>(this);
    }

    if (!LeaderboardSystem)
    {
        LeaderboardSystem = NewObject<ULeaderboardSystem>(this);
    }

    if (!TutorialSystem)
    {
        TutorialSystem = NewObject<UTutorialSystem>(this);
        TutorialSystem->Steps = {
            { TEXT("move"), TEXT("Getting Moving"), TEXT("Use W and S to accelerate and brake."), TEXT("Throttle"), 0.0f, true },
            { TEXT("steer"), TEXT("Steering"), TEXT("Use A and D to steer left and right."), TEXT("Steer"), 0.0f, true },
            { TEXT("handbrake"), TEXT("Drifting"), TEXT("Press Space to use the handbrake for tight corners."), TEXT("Handbrake"), 0.0f, true },
            { TEXT("checkpoint"), TEXT("Checkpoints"), TEXT("Drive through the glowing gates to progress."), TEXT("Throttle"), 5.0f, false },
            { TEXT("create"), TEXT("Create Your World"), TEXT("Open the menu and create your own race route!"), TEXT("Throttle"), 5.0f, false },
            { TEXT("finish"), TEXT("Good Luck!"), TEXT("Complete the route as fast as you can."), TEXT("Throttle"), 3.0f, false }
        };
    }

    if (!AchievementSystem)
    {
        AchievementSystem = NewObject<UAchievementSystem>(this);
        AchievementSystem->InitializeAchievements();
    }

    if (!ConsoleCommands)
    {
        ConsoleCommands = NewObject<UConsoleCommands>(this);
    }

    CreateDefaultVehiclePresets();
    LoadHandlingModePresets();

    // Restore selected vehicle from game instance
    if (UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance()))
    {
        if (GI->LastSelectedVehicleTuning)
        {
            SelectedVehicleTuning = GI->LastSelectedVehicleTuning;
        }
        else if (VehiclePresets.Num() > 0)
        {
            // Try to match by name
            for (UVehicleTuningData* Preset : VehiclePresets)
            {
                if (Preset && Preset->DisplayName == GI->LastSelectedVehicle)
                {
                    SelectedVehicleTuning = Preset;
                    break;
                }
            }
            if (!SelectedVehicleTuning)
            {
                SelectedVehicleTuning = VehiclePresets[0];
            }
        }

        const FString HandlingMode = GI->LastSelectedHandlingMode.IsEmpty()
            ? TEXT("Arcade")
            : GI->LastSelectedHandlingMode;
        SelectedVehicleTuning = BuildMergedVehicleTuning(SelectedVehicleTuning, HandlingMode);
        GI->LastSelectedVehicleTuning = SelectedVehicleTuning;
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
        RoadGen->XodrPath = !CityLayout.XodrPath.IsEmpty()
            ? CityLayout.XodrPath
            : CityPackPath + XodrFile;
        RoadGen->GenerateRoadMeshAsync();
    }

    // Spawn building generator
    if (BuildingGeneratorClass)
    {
        ABuildingMeshGenerator* BuildingGen = GetWorld()->SpawnActor<ABuildingMeshGenerator>(
            BuildingGeneratorClass, FVector::ZeroVector, FRotator::ZeroRotator, RoadParams);
        if (BuildingGen)
        {
            BuildingGen->BuildingsJsonPath = !CityLayout.BuildingsPath.IsEmpty()
                ? CityLayout.BuildingsPath
                : CityPackPath + TEXT("akron_buildings.json");
            BuildingGen->GenerateBuildingsAsync();
        }
    }

    // Spawn street furniture
    if (FurnitureSpawnerClass)
    {
        AStreetFurnitureSpawner* Furniture = GetWorld()->SpawnActor<AStreetFurnitureSpawner>(
            FurnitureSpawnerClass, FVector::ZeroVector, FRotator::ZeroRotator, RoadParams);
        if (Furniture)
        {
            Furniture->RoadGraphJsonPath = !CityLayout.RoadGraphPath.IsEmpty()
                ? CityLayout.RoadGraphPath
                : CityPackPath + TEXT("akron_road_graph.json");
            Furniture->SpawnFurnitureAsync();
        }
    }

    // After road generation + brief load, transition to countdown
    FTimerHandle LoadTimer;
    GetWorld()->GetTimerManager().SetTimer(LoadTimer, [this]()
    {
        if (LoadingScreen)
        {
            LoadingScreen->SetProgress(1.0f);
            LoadingScreen->SetStatusText(TEXT("Ready!"));
            LoadingScreen->FinishLoading();
        }
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

    if (LoadingScreenClass)
    {
        LoadingScreen = CreateWidget<ULoadingScreenWidget>(PC, LoadingScreenClass);
        if (LoadingScreen)
        {
            LoadingScreen->AddToViewport(200);
            LoadingScreen->StartLoading();
        }
    }

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

bool ACruiseSprintGameMode::IsVersionCompatible(const FString& CityVersion) const
{
    // Simple semver check: major.minor must match
    FString GameVersion = FString(RACEGPS_VERSION_STRING);
    TArray<FString> GameParts;
    GameVersion.ParseIntoArray(GameParts, TEXT("."));
    TArray<FString> CityParts;
    CityVersion.ParseIntoArray(CityParts, TEXT("."));

    if (GameParts.Num() < 2 || CityParts.Num() < 2)
        return true; // Be lenient if parsing fails

    return GameParts[0] == CityParts[0] && GameParts[1] == CityParts[1];
}

void ACruiseSprintGameMode::LoadCityData()
{
    // Manifest path comes from the resolved layout when available.
    const FString ManifestPath = !CityLayout.ManifestPath.IsEmpty()
        ? CityLayout.ManifestPath
        : CityPackPath + ManifestFile;
    UAkronXodrImporter::LoadManifest(ManifestPath, WorldOriginLat, WorldOriginLon);

    // Routes: single array file resolved from the manifest (both dialects), with the
    // legacy per-route directory as fallback.
    if (!CityLayout.RoutesPath.IsEmpty())
    {
        UAkronXodrImporter::LoadRouteSplines(CityLayout.RoutesPath, LoadedRoutes);
    }
    else
    {
        UAkronXodrImporter::LoadRouteSplines(RouteDir, LoadedRoutes);
    }
    UAkronXodrImporter::LoadSpawnPoints(ManifestPath, LoadedSpawns);
    UAkronXodrImporter::LoadPOIs(ManifestPath, LoadedPOIs);

    // Version compatibility check
    FString FullManifestPath = FPaths::ProjectDir() / ManifestPath;
    FString Content;
    if (FFileHelper::LoadFileToString(Content, *FullManifestPath))
    {
        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        if (FJsonSerializer::Deserialize(Reader, Root))
        {
            FString CityVersion;
            if (Root->TryGetStringField(TEXT("version"), CityVersion))
            {
                if (!IsVersionCompatible(CityVersion))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Citypack version %s may be incompatible with game %s"),
                        *CityVersion, *FString(RACEGPS_VERSION_STRING));
                }
                else
                {
                    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Citypack version %s is compatible"), *CityVersion);
                }
            }
        }
    }

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

    // Apply selected vehicle tuning after spawn/teleport
    ApplyVehicleTuningToPlayer();
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

    // Start tutorial on first race
    if (TutorialSystem && !TutorialSystem->IsActive())
    {
        TutorialSystem->StartTutorial();

        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
        if (PC && TutorialWidgetClass)
        {
            UTutorialWidget* TutWidget = CreateWidget<UTutorialWidget>(PC, TutorialWidgetClass);
            if (TutWidget)
            {
                FTutorialStep Step = TutorialSystem->GetCurrentStep();
                TutWidget->ShowStep(Step.Title, Step.Description, Step.InputAction);
                TutWidget->AddToViewport(60);
            }
        }
    }

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

    FString RouteId;
    if (LoadedRoutes.Num() > 0 && SelectedRouteIndex < LoadedRoutes.Num())
    {
        RouteId = LoadedRoutes[SelectedRouteIndex].RouteId;
    }

    if (ScoringSystem)
    {
        FRaceScore Score = ScoringSystem->CalculateFinalScore(ElapsedTime);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Race finished! Base: %.2fs, Penalties: %.2fs, Bonus: %.2fs, Final: %.2fs, Medal: %s"),
            Score.BaseTime, Score.CollisionPenalty + Score.MissedCheckpointPenalty, Score.CleanDrivingBonus,
            Score.FinalTime, *Score.Medal);

        UraceGPSGameInstance* GI = Cast<UraceGPSGameInstance>(GetGameInstance());
        if (GI && !RouteId.IsEmpty())
        {
            GI->UpdateBestTime(RouteId, Score.FinalTime);
        }

        // Add leaderboard entry
        if (LeaderboardSystem && !RouteId.IsEmpty())
        {
            if (!LeaderboardSystem->HasLeaderboard(RouteId))
            {
                LeaderboardSystem->SeedDefaultEntries(RouteId, GoldTimeSeconds, SilverTimeSeconds, BronzeTimeSeconds);
            }

            FLeaderboardEntry Entry;
            Entry.PlayerName = TEXT("Player");
            Entry.TimeSeconds = Score.FinalTime;
            Entry.Medal = Score.Medal;
            Entry.Date = FDateTime::Now().ToString(TEXT("%Y-%m-%d"));
            Entry.VehicleUsed = TEXT("Sedan");
            Entry.Collisions = Score.Collisions;
            Entry.bIsPlayer = true;
            LeaderboardSystem->AddEntry(RouteId, Entry);
        }
    }

    // Save replay if it's the best
    if (ReplayManager && !RouteId.IsEmpty())
    {
        ReplayManager->EndRaceRecording();

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

void ACruiseSprintGameMode::StartRaceForAllPlayers()
{
    // In multiplayer, host triggers this and it replicates to all clients
    if (HasAuthority())
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

        // Spawn all players at their start positions
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (PC && PC->GetPawn())
            {
                SpawnPlayerAtStart();
            }
        }

        OnRaceStateChanged(CurrentState);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Multiplayer race started for all players"));
    }
}

void ACruiseSprintGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Player joined. Total players: %d"), GetWorld()->GetNumPlayerControllers());

    // If race is already in progress, teleport new player to start
    if (CurrentState == ECruiseSprintState::Racing || CurrentState == ECruiseSprintState::Countdown)
    {
        if (NewPlayer && NewPlayer->GetPawn())
        {
            SpawnPlayerAtStart();
        }
    }
}

void ACruiseSprintGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Player left. Total players: %d"), GetWorld()->GetNumPlayerControllers());
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

    // Advance tutorial on state changes
    if (TutorialSystem && TutorialSystem->IsActive())
    {
        if (NewState == ECruiseSprintState::Racing)
        {
            // Tutorial auto-advances when race starts
        }
    }
}

void ACruiseSprintGameMode::CreateDefaultVehiclePresets()
{
    VehiclePresets.Empty();

    auto CreatePreset = [&](const FString& Name, const FString& Desc, EVehicleClass VClass,
                            float Mass, float MaxRPM, float Drag, int32 Gears,
                            float BrakeTorque, float HandbrakeTorque,
                            float SteerCurve, float Downforce) -> UVehicleTuningData*
    {
        UVehicleTuningData* Preset = NewObject<UVehicleTuningData>(this);
        Preset->DisplayName = Name;
        Preset->Description = Desc;
        Preset->VehicleClass = VClass;
        Preset->VehicleMass = Mass;
        Preset->MaxEngineRPM = MaxRPM;
        Preset->IdleRPM = 800.0f;
        Preset->DragCoefficient = Drag;
        Preset->BrakeTorque = BrakeTorque;
        Preset->HandbrakeTorque = HandbrakeTorque;
        Preset->SteeringCurve = SteerCurve;
        Preset->AckermannAccuracy = 1.0f;
        Preset->DownForceCoefficient = Downforce;
        Preset->DownForceOffset = 0.0f;
        Preset->ChassisWidth = 180.0f;
        Preset->ChassisHeight = 140.0f;

        // Transmission
        Preset->Transmission.FinalDriveRatio = 3.5f;
        Preset->Transmission.ReverseGearRatio = -3.0f;
        Preset->Transmission.UpShiftRPM = MaxRPM * 0.85f;
        Preset->Transmission.DownShiftRPM = MaxRPM * 0.3f;
        Preset->Transmission.ChangeUpTime = 0.3f;
        Preset->Transmission.ChangeDownTime = 0.3f;

        // Gear ratios based on gear count
        if (Gears == 5)
        {
            Preset->Transmission.GearRatios = { 3.5f, 2.0f, 1.3f, 0.9f, 0.7f };
        }
        else if (Gears == 6)
        {
            Preset->Transmission.GearRatios = { 3.8f, 2.2f, 1.5f, 1.1f, 0.85f, 0.65f };
        }
        else
        {
            Preset->Transmission.GearRatios = { 4.0f, 2.5f, 1.6f, 1.2f, 0.9f, 0.7f, 0.55f };
        }

        // Differential
        Preset->Differential.DifferentialType = ERaceGPSDifferentialType::AllWheelDrive;
        Preset->Differential.FrontRearSplit = 0.5f;
        Preset->Differential.FrontLeftRightSplit = 0.5f;
        Preset->Differential.RearLeftRightSplit = 0.5f;
        Preset->Differential.CentreBias = 1.3f;
        Preset->Differential.FrontBias = 1.3f;
        Preset->Differential.RearBias = 1.3f;

        // Wheels (4-wheel setup)
        FWheelTuning FrontWheel;
        FrontWheel.Radius = 35.0f;
        FrontWheel.Width = 20.0f;
        FrontWheel.Mass = 20.0f;
        FrontWheel.SteerAngle = 30.0f;
        FrontWheel.bDrive = true;
        FrontWheel.bHandbrake = false;
        FrontWheel.SuspensionStiffness = 450.0f;
        FrontWheel.SuspensionDamping = 25.0f;
        FrontWheel.MaxRaise = 10.0f;
        FrontWheel.MaxDrop = 10.0f;

        FWheelTuning RearWheel;
        RearWheel.Radius = 35.0f;
        RearWheel.Width = 20.0f;
        RearWheel.Mass = 20.0f;
        RearWheel.SteerAngle = 0.0f;
        RearWheel.bDrive = true;
        RearWheel.bHandbrake = true;
        RearWheel.SuspensionStiffness = 450.0f;
        RearWheel.SuspensionDamping = 25.0f;
        RearWheel.MaxRaise = 10.0f;
        RearWheel.MaxDrop = 10.0f;

        Preset->Wheels.Add(FrontWheel);
        Preset->Wheels.Add(FrontWheel);
        Preset->Wheels.Add(RearWheel);
        Preset->Wheels.Add(RearWheel);

        return Preset;
    };

    // Sedan — Balanced all-rounder
    VehiclePresets.Add(CreatePreset(
        TEXT("Sedan"), TEXT("Balanced handling with moderate speed and grip. Great for learning."),
        EVehicleClass::Sedan,
        1500.0f, 7000.0f, 0.30f, 5,
        1500.0f, 3000.0f,
        0.5f, 0.1f
    ));

    // Sports — Fast, light, drift-happy
    VehiclePresets.Add(CreatePreset(
        TEXT("Sports"), TEXT("Lightweight and powerful. High top speed, lower grip. Drift king."),
        EVehicleClass::Sports,
        1200.0f, 8500.0f, 0.25f, 6,
        1800.0f, 3500.0f,
        0.4f, 0.15f
    ));

    // Truck — Heavy, slow, high grip
    VehiclePresets.Add(CreatePreset(
        TEXT("Truck"), TEXT("Heavy and stable. Slow acceleration but excellent grip and braking."),
        EVehicleClass::Truck,
        2500.0f, 5500.0f, 0.45f, 5,
        2500.0f, 4000.0f,
        0.6f, 0.05f
    ));

    // CARLA Charger — imported CARLA 0.10.0 Dodge Charger 2024 hero vehicle
    // (CC-BY 4.0, see Content/Vehicles/CARLA-ATTRIBUTION.txt). Muscle-sports
    // tuning paired with BP_DodgeCharger2024 (child of AChaosVehiclePawn, so
    // the arcade handling defaults apply).
    VehiclePresets.Add(CreatePreset(
        TEXT("CARLA Charger"), TEXT("CARLA Dodge Charger 2024. Big V8 muscle: heavy, torquey, built for power slides."),
        EVehicleClass::Sports,
        1900.0f, 6500.0f, 0.35f, 6,
        2000.0f, 3800.0f,
        0.45f, 0.12f
    ));

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Created %d vehicle presets"), VehiclePresets.Num());
}

void ACruiseSprintGameMode::LoadHandlingModePresets()
{
    HandlingModePresets.Empty();

    const FString PresetDir = FPaths::ProjectDir() / TEXT("Content/Data/VehiclePresets");
    const TArray<FString> PresetFiles = {
        TEXT("Arcade.json"),
        TEXT("Drift.json"),
        TEXT("Simulation.json")
    };

    for (const FString& FileName : PresetFiles)
    {
        FString Content;
        const FString FullPath = PresetDir / FileName;
        if (!FFileHelper::LoadFileToString(Content, *FullPath))
        {
            UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Failed to load handling preset: %s"), *FullPath);
            continue;
        }

        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Failed to parse handling preset: %s"), *FullPath);
            continue;
        }

        UVehicleTuningData* Preset = NewObject<UVehicleTuningData>(this);
        Preset->DisplayName = Root->GetStringField(TEXT("PresetName"));
        Root->TryGetStringField(TEXT("Description"), Preset->Description);

        double Num = 0.0;
        if (Root->TryGetNumberField(TEXT("VehicleMass"), Num)) Preset->VehicleMass = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("DragCoefficient"), Num)) Preset->DragCoefficient = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("ChassisWidth"), Num)) Preset->ChassisWidth = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("ChassisHeight"), Num)) Preset->ChassisHeight = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("MaxEngineRPM"), Num)) Preset->MaxEngineRPM = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("IdleRPM"), Num)) Preset->IdleRPM = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("BrakeTorque"), Num)) Preset->BrakeTorque = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("HandbrakeTorque"), Num)) Preset->HandbrakeTorque = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("DownForceCoefficient"), Num)) Preset->DownForceCoefficient = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("DownForceOffset"), Num)) Preset->DownForceOffset = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("SteeringCurve"), Num)) Preset->SteeringCurve = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("AckermannAccuracy"), Num)) Preset->AckermannAccuracy = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("DriftAngleMax"), Num)) Preset->DriftAngleMax = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("CounterSteerGain"), Num)) Preset->CounterSteerGain = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("HandbrakeDriftFactor"), Num)) Preset->HandbrakeDriftFactor = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("TractionControl"), Num)) Preset->TractionControl = static_cast<float>(Num);
        if (Root->TryGetNumberField(TEXT("StabilityControl"), Num)) Preset->StabilityControl = static_cast<float>(Num);

        const TSharedPtr<FJsonObject>* Transmission = nullptr;
        if (Root->TryGetObjectField(TEXT("Transmission"), Transmission) && Transmission)
        {
            if ((*Transmission)->TryGetNumberField(TEXT("FinalDriveRatio"), Num)) Preset->Transmission.FinalDriveRatio = static_cast<float>(Num);
            if ((*Transmission)->TryGetNumberField(TEXT("ReverseGearRatio"), Num)) Preset->Transmission.ReverseGearRatio = static_cast<float>(Num);
            if ((*Transmission)->TryGetNumberField(TEXT("UpShiftRPM"), Num)) Preset->Transmission.UpShiftRPM = static_cast<float>(Num);
            if ((*Transmission)->TryGetNumberField(TEXT("DownShiftRPM"), Num)) Preset->Transmission.DownShiftRPM = static_cast<float>(Num);
            if ((*Transmission)->TryGetNumberField(TEXT("ChangeUpTime"), Num)) Preset->Transmission.ChangeUpTime = static_cast<float>(Num);
            if ((*Transmission)->TryGetNumberField(TEXT("ChangeDownTime"), Num)) Preset->Transmission.ChangeDownTime = static_cast<float>(Num);
            const TArray<TSharedPtr<FJsonValue>>* GearRatios = nullptr;
            if ((*Transmission)->TryGetArrayField(TEXT("GearRatios"), GearRatios) && GearRatios)
            {
                Preset->Transmission.GearRatios.Empty();
                for (const TSharedPtr<FJsonValue>& Ratio : *GearRatios)
                {
                    double GearValue = 0.0;
                    if (Ratio->TryGetNumber(GearValue))
                    {
                        Preset->Transmission.GearRatios.Add(static_cast<float>(GearValue));
                    }
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* Wheels = nullptr;
        if (Root->TryGetArrayField(TEXT("Wheels"), Wheels) && Wheels)
        {
            Preset->Wheels.Empty();
            for (const TSharedPtr<FJsonValue>& WheelValue : *Wheels)
            {
                const TSharedPtr<FJsonObject>* WheelObj = nullptr;
                if (!WheelValue->TryGetObject(WheelObj) || !WheelObj)
                {
                    continue;
                }
                FWheelTuning Wheel;
                if ((*WheelObj)->TryGetNumberField(TEXT("Radius"), Num)) Wheel.Radius = static_cast<float>(Num);
                if ((*WheelObj)->TryGetNumberField(TEXT("Width"), Num)) Wheel.Width = static_cast<float>(Num);
                if ((*WheelObj)->TryGetNumberField(TEXT("Mass"), Num)) Wheel.Mass = static_cast<float>(Num);
                if ((*WheelObj)->TryGetNumberField(TEXT("SteerAngle"), Num)) Wheel.SteerAngle = static_cast<float>(Num);
                if ((*WheelObj)->TryGetNumberField(TEXT("SuspensionStiffness"), Num)) Wheel.SuspensionStiffness = static_cast<float>(Num);
                if ((*WheelObj)->TryGetNumberField(TEXT("SuspensionDamping"), Num)) Wheel.SuspensionDamping = static_cast<float>(Num);
                if ((*WheelObj)->TryGetNumberField(TEXT("MaxRaise"), Num)) Wheel.MaxRaise = static_cast<float>(Num);
                if ((*WheelObj)->TryGetNumberField(TEXT("MaxDrop"), Num)) Wheel.MaxDrop = static_cast<float>(Num);
                if ((*WheelObj)->TryGetNumberField(TEXT("SuspensionForceOffset"), Num)) Wheel.SuspensionForceOffset = static_cast<float>(Num);
                (*WheelObj)->TryGetBoolField(TEXT("bDrive"), Wheel.bDrive);
                (*WheelObj)->TryGetBoolField(TEXT("bHandbrake"), Wheel.bHandbrake);
                Preset->Wheels.Add(Wheel);
            }
        }

        HandlingModePresets.Add(Preset->DisplayName, Preset);
    }
}

TObjectPtr<UVehicleTuningData> ACruiseSprintGameMode::BuildMergedVehicleTuning(UVehicleTuningData* BaseVehiclePreset, const FString& HandlingMode)
{
    UVehicleTuningData* BasePreset = BaseVehiclePreset;
    if (!BasePreset && VehiclePresets.Num() > 0)
    {
        BasePreset = VehiclePresets[0];
    }
    if (!BasePreset)
    {
        return nullptr;
    }

    TObjectPtr<UVehicleTuningData>* HandlingPresetPtr = HandlingModePresets.Find(HandlingMode);
    UVehicleTuningData* HandlingPreset = HandlingPresetPtr ? HandlingPresetPtr->Get() : nullptr;
    if (!HandlingPreset)
    {
        return BasePreset;
    }

    const float BehaviorBlend = HandlingMode == TEXT("Simulation") ? 0.35f
        : (HandlingMode == TEXT("Drift") ? 0.85f : 0.6f);

    UVehicleTuningData* Merged = NewObject<UVehicleTuningData>(this, UVehicleTuningData::StaticClass(), NAME_None, RF_NoFlags, BasePreset);
    Merged->DisplayName = FString::Printf(TEXT("%s / %s"), *BasePreset->DisplayName, *HandlingMode);
    Merged->Description = FString::Printf(TEXT("%s | %s"), *BasePreset->Description, *HandlingPreset->Description);

    auto BlendValue = [](float BaseValue, float HandlingValue, float Blend)
    {
        return FMath::Lerp(BaseValue, HandlingValue, Blend);
    };

    // Keep the vehicle archetype identity intact.
    Merged->VehicleClass = BasePreset->VehicleClass;
    Merged->VehicleMass = BasePreset->VehicleMass;
    Merged->DragCoefficient = BlendValue(BasePreset->DragCoefficient, HandlingPreset->DragCoefficient, BehaviorBlend * 0.35f);
    Merged->ChassisWidth = BasePreset->ChassisWidth;
    Merged->ChassisHeight = BasePreset->ChassisHeight;
    Merged->MaxEngineRPM = BlendValue(BasePreset->MaxEngineRPM, HandlingPreset->MaxEngineRPM, BehaviorBlend * 0.4f);
    Merged->IdleRPM = BlendValue(BasePreset->IdleRPM, HandlingPreset->IdleRPM, BehaviorBlend * 0.25f);
    Merged->BrakeTorque = BlendValue(BasePreset->BrakeTorque, HandlingPreset->BrakeTorque, BehaviorBlend);
    Merged->HandbrakeTorque = BlendValue(BasePreset->HandbrakeTorque, HandlingPreset->HandbrakeTorque, BehaviorBlend);
    Merged->DownForceCoefficient = BlendValue(BasePreset->DownForceCoefficient, HandlingPreset->DownForceCoefficient, BehaviorBlend * 0.6f);
    Merged->DownForceOffset = BlendValue(BasePreset->DownForceOffset, HandlingPreset->DownForceOffset, BehaviorBlend * 0.5f);
    // SteeringCurve is a curve asset; keep the base preset curve rather than blending.
    Merged->AckermannAccuracy = BlendValue(BasePreset->AckermannAccuracy, HandlingPreset->AckermannAccuracy, BehaviorBlend * 0.7f);
    Merged->DriftAngleMax = BlendValue(BasePreset->DriftAngleMax, HandlingPreset->DriftAngleMax, BehaviorBlend);
    Merged->CounterSteerGain = BlendValue(BasePreset->CounterSteerGain, HandlingPreset->CounterSteerGain, BehaviorBlend);
    Merged->HandbrakeDriftFactor = BlendValue(BasePreset->HandbrakeDriftFactor, HandlingPreset->HandbrakeDriftFactor, BehaviorBlend);
    Merged->TractionControl = BlendValue(BasePreset->TractionControl, HandlingPreset->TractionControl, BehaviorBlend);
    Merged->StabilityControl = BlendValue(BasePreset->StabilityControl, HandlingPreset->StabilityControl, BehaviorBlend);

    // Blend transmission behavior, but keep gear count / base progression stable.
    Merged->Transmission = BasePreset->Transmission;
    Merged->Transmission.FinalDriveRatio = FMath::Lerp(BasePreset->Transmission.FinalDriveRatio, HandlingPreset->Transmission.FinalDriveRatio, BehaviorBlend * 0.5f);
    Merged->Transmission.ReverseGearRatio = FMath::Lerp(BasePreset->Transmission.ReverseGearRatio, HandlingPreset->Transmission.ReverseGearRatio, BehaviorBlend * 0.5f);
    Merged->Transmission.UpShiftRPM = FMath::Lerp(BasePreset->Transmission.UpShiftRPM, HandlingPreset->Transmission.UpShiftRPM, BehaviorBlend * 0.6f);
    Merged->Transmission.DownShiftRPM = FMath::Lerp(BasePreset->Transmission.DownShiftRPM, HandlingPreset->Transmission.DownShiftRPM, BehaviorBlend * 0.6f);
    Merged->Transmission.ChangeUpTime = FMath::Lerp(BasePreset->Transmission.ChangeUpTime, HandlingPreset->Transmission.ChangeUpTime, BehaviorBlend * 0.75f);
    Merged->Transmission.ChangeDownTime = FMath::Lerp(BasePreset->Transmission.ChangeDownTime, HandlingPreset->Transmission.ChangeDownTime, BehaviorBlend * 0.75f);

    // Blend differential behavior selectively instead of replacing the whole driveline identity.
    Merged->Differential = BasePreset->Differential;
    Merged->Differential.FrontRearSplit = FMath::Lerp(BasePreset->Differential.FrontRearSplit, HandlingPreset->Differential.FrontRearSplit, BehaviorBlend * 0.65f);
    Merged->Differential.FrontLeftRightSplit = FMath::Lerp(BasePreset->Differential.FrontLeftRightSplit, HandlingPreset->Differential.FrontLeftRightSplit, BehaviorBlend * 0.65f);
    Merged->Differential.RearLeftRightSplit = FMath::Lerp(BasePreset->Differential.RearLeftRightSplit, HandlingPreset->Differential.RearLeftRightSplit, BehaviorBlend * 0.65f);
    Merged->Differential.CentreBias = FMath::Lerp(BasePreset->Differential.CentreBias, HandlingPreset->Differential.CentreBias, BehaviorBlend * 0.65f);
    Merged->Differential.FrontBias = FMath::Lerp(BasePreset->Differential.FrontBias, HandlingPreset->Differential.FrontBias, BehaviorBlend * 0.65f);
    Merged->Differential.RearBias = FMath::Lerp(BasePreset->Differential.RearBias, HandlingPreset->Differential.RearBias, BehaviorBlend * 0.65f);

    // Preserve wheel layout and archetype stance; only tune suspension/steering feel per wheel.
    Merged->Wheels = BasePreset->Wheels;
    const int32 WheelCount = FMath::Min(Merged->Wheels.Num(), HandlingPreset->Wheels.Num());
    for (int32 WheelIndex = 0; WheelIndex < WheelCount; ++WheelIndex)
    {
        FWheelTuning& BaseWheel = Merged->Wheels[WheelIndex];
        const FWheelTuning& HandlingWheel = HandlingPreset->Wheels[WheelIndex];
        BaseWheel.SteerAngle = BlendValue(BaseWheel.SteerAngle, HandlingWheel.SteerAngle, BehaviorBlend);
        BaseWheel.SuspensionStiffness = BlendValue(BaseWheel.SuspensionStiffness, HandlingWheel.SuspensionStiffness, BehaviorBlend * 0.7f);
        BaseWheel.SuspensionDamping = BlendValue(BaseWheel.SuspensionDamping, HandlingWheel.SuspensionDamping, BehaviorBlend * 0.7f);
        BaseWheel.MaxRaise = BlendValue(BaseWheel.MaxRaise, HandlingWheel.MaxRaise, BehaviorBlend * 0.5f);
        BaseWheel.MaxDrop = BlendValue(BaseWheel.MaxDrop, HandlingWheel.MaxDrop, BehaviorBlend * 0.5f);
        BaseWheel.SuspensionForceOffset = BlendValue(BaseWheel.SuspensionForceOffset, HandlingWheel.SuspensionForceOffset, BehaviorBlend * 0.5f);
    }

    return Merged;
}

void ACruiseSprintGameMode::ApplyVehicleTuningToPlayer()
{
    if (!SelectedVehicleTuning)
        return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
        return;

    AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(PC->GetPawn());
    if (Vehicle)
    {
        Vehicle->SetTuningData(SelectedVehicleTuning);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Applied vehicle tuning: %s"), *SelectedVehicleTuning->DisplayName);
    }
}
