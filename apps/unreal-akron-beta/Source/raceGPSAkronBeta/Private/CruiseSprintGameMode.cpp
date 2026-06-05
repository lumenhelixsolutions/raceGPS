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

    // Spawn building generator
    if (BuildingGeneratorClass)
    {
        ABuildingMeshGenerator* BuildingGen = GetWorld()->SpawnActor<ABuildingMeshGenerator>(
            BuildingGeneratorClass, FVector::ZeroVector, FRotator::ZeroRotator, RoadParams);
        if (BuildingGen)
        {
            BuildingGen->BuildingsJsonPath = CityPackPath + TEXT("akron_buildings.json");
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
            Furniture->RoadGraphJsonPath = CityPackPath + TEXT("akron_road_graph.json");
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
    FString ManifestPath = CityPackPath + ManifestFile;
    UAkronXodrImporter::LoadManifest(ManifestPath, WorldOriginLat, WorldOriginLon);

    UAkronXodrImporter::LoadRouteSplines(RouteDir, LoadedRoutes);
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
                        *CityVersion, FString(RACEGPS_VERSION_STRING));
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
        Preset->Differential.DifferentialType = EVehicleDifferential::LimitedSlip_4W;
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

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Created %d vehicle presets"), VehiclePresets.Num());
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
