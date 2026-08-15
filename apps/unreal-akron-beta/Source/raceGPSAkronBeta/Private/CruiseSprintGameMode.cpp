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
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Components/SplineComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/App.h"

// Defined at the bottom of this file (diagnostics section); used earlier by the
// spawn-clearance guard.
static FString DiagActorName(const AActor* Actor);

// racegps.Diagnostics: 1 = runtime self-diagnostics (preflight + 15s ticker).
// Default ON in Development builds; override via [SystemSettings] in
// DefaultEngine.ini or -racegps.Diagnostics=0 on the command line.
static TAutoConsoleVariable<int32> CVarRaceGPSDiagnostics(
    TEXT("racegps.Diagnostics"),
#if UE_BUILD_DEVELOPMENT
    1,
#else
    0,
#endif
    TEXT("1 = emit [raceGPS-PREFLIGHT]/[raceGPS-DIAG] runtime self-diagnostics for the first 15s after StartPlay"),
    ECVF_Default);

ACruiseSprintGameMode::ACruiseSprintGameMode(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Hero vehicle: CARLA Dodge Charger 2024 Blueprint (T5). In UE 5.7
    // AGameModeBase::DefaultPawnClass is NOT a config property, so the class
    // must be referenced here, not via ini. Fall back to the bare
    // AChaosVehiclePawn if the BP is missing (content not staged).
    static ConstructorHelpers::FClassFinder<APawn> HeroPawnBP(
        TEXT("/Game/Vehicles/DodgeCharger2024/BP_DodgeCharger2024"));
    DefaultPawnClass = HeroPawnBP.Succeeded()
        ? HeroPawnBP.Class.Get()
        : AChaosVehiclePawn::StaticClass();
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

    // --- Baked-map detection (black-screen hotfix) ---------------------------
    // When the loaded level IS the baked city map for the active city, the map
    // already contains PlayerStarts, route splines and checkpoint gates at
    // correct baked (UE Z-up) positions; runtime spawning must stand down or
    // every gate/spline is duplicated. Detection: persistent-level package
    // short name == resolved LevelName (PIE "UEDPIE_<n>_" prefix stripped).
    {
        FString MapName = FPackageName::GetShortName(GetWorld()->GetOutermost()->GetName());
        if (MapName.StartsWith(TEXT("UEDPIE_")))
        {
            const int32 Sep = MapName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 7);
            if (Sep != INDEX_NONE)
            {
                MapName.RightChopInline(Sep + 1);
            }
        }
        bRunningBakedCityMap = !CityLayout.LevelName.IsEmpty() &&
            MapName.Equals(CityLayout.LevelName, ESearchCase::IgnoreCase);
        int32 BakedSplines = 0;
        int32 BakedGates = 0;
        for (TActorIterator<ARouteSplineActor> It(GetWorld()); It; ++It) { BakedSplines++; }
        for (TActorIterator<ACheckpointGate> It(GetWorld()); It; ++It) { BakedGates++; }
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Map '%s' vs city level '%s' -> baked city map: %s (baked splines: %d, baked gates: %d)"),
            *MapName, *CityLayout.LevelName, bRunningBakedCityMap ? TEXT("YES") : TEXT("no"), BakedSplines, BakedGates);
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
    {        if (GI->LastSelectedVehicleTuning)
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
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Vehicle selection: %s"),
            SelectedVehicleTuning ? *SelectedVehicleTuning->DisplayName : TEXT("<none>"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] GameInstance class is '%s', expected raceGPSGameInstance — vehicle selection inert"),
            *GetNameSafe(GetGameInstance() ? GetGameInstance()->GetClass() : nullptr));
    }

    LoadCityData();
    CurrentState = ECruiseSprintState::Loading;

    if (bRunningBakedCityMap)
    {
        // Baked maps carry terrain/buildings/water in the .umap (T10). The
        // runtime generators target the legacy flat meter-scale path (and
        // StreetFurnitureSpawner is hardcoded to the Akron origin), so they
        // would only produce invisible or misplaced duplicates here.
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Baked map: runtime road/building/furniture generation skipped (city content is baked)"));
    }
    else
    {
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

    // --- Runtime self-diagnostics (racegps.Diagnostics) ---
    if (IsDiagnosticsEnabled())
    {
        RunDiagnosticsPreflight();
        DiagSampleCount = 0;
        bDiagShotDone = false;
        DiagGatesBound = -1;
        GetWorld()->GetTimerManager().SetTimer(DiagTimerHandle, this,
            &ACruiseSprintGameMode::DiagnosticsSampleTick, 1.0f, true, 1.0f);
    }
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
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;

    if (bRunningBakedCityMap)
    {
        // Baked PlayerStarts are already at correct positions; stock
        // RestartPlayer placed the pawn there. Only re-apply vehicle tuning.
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Baked map: pawn at %s (runtime teleport skipped)"),
            Pawn ? *Pawn->GetActorLocation().ToString() : TEXT("<no pawn>"));
        ApplyVehicleTuningToPlayer();
        return;
    }

    if (LoadedSpawns.Num() == 0) return;

    FAkronSpawnPoint& Spawn = LoadedSpawns[0];
    // Spawn.Location stores (lon, 0, -lat) in degrees (compiler convention),
    // so Lat = -Location.Z, Lon = Location.X. GeoToWorld is now UE Z-up.
    FVector WorldLoc = UAkronXodrImporter::GeoToWorld(
        -Spawn.Location.Z, Spawn.Location.X, WorldOriginLat, WorldOriginLon);
    WorldLoc.Z = 50.0f; // Slight lift off ground

    if (Pawn)
    {
        Pawn->SetActorLocationAndRotation(WorldLoc, Spawn.Rotation, false, nullptr, ETeleportType::ResetPhysics);
    }

    // Apply selected vehicle tuning after spawn/teleport
    ApplyVehicleTuningToPlayer();
}

void ACruiseSprintGameMode::SpawnRouteSpline()
{
    if (bRunningBakedCityMap)
    {
        // The selected route's spline is baked into the map; only the ghost
        // needs runtime wiring, following the baked spline's world points.
        SpawnGhostOnBakedRoute();
        return;
    }

    if (LoadedRoutes.Num() == 0 || SelectedRouteIndex >= LoadedRoutes.Num()) return;

    const FAkronRouteSpline& Route = LoadedRoutes[SelectedRouteIndex];
    if (Route.Waypoints.Num() < 2) return;

    // Convert raw lat/lon waypoints to world space. Waypoints store
    // (lon, ?, -lat) in degrees, so Lat = -Wp.Z, Lon = Wp.X.
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
    if (bRunningBakedCityMap)
    {
        // Gates are baked into the map; index/activate/bind them in place.
        BindBakedCheckpointGates();
        return;
    }

    if (LoadedRoutes.Num() == 0 || SelectedRouteIndex >= LoadedRoutes.Num()) return;

    const FAkronRouteSpline& Route = LoadedRoutes[SelectedRouteIndex];
    int32 Spawned = 0;
    for (int32 i = 0; i < Route.CheckpointLocations.Num(); ++i)
    {
        // CheckpointLocations store (lon, ?, -lat) in degrees.
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
            Spawned++;
        }
    }
    DiagnosticsReportGates(Spawned, Route.CheckpointLocations.Num());
}

APawn* ACruiseSprintGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
    FActorSpawnParameters SpawnInfo;
    SpawnInfo.Instigator = GetInstigator();
    SpawnInfo.ObjectFlags |= RF_Transient; // never save default player pawns into a map
    SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // --- Spawn-clearance guard -------------------------------------------------
    // PlayerStart Z comes from spec data; the baked terrain heightfield can sit
    // decimeters ABOVE it, embedding the car under the surface (camera under
    // terrain -> fully black viewport; up-trace catches it, down-trace cannot).
    // Probe the column above/below the spawn: if the nearest surface from above
    // lies above the spawn origin, lift the spawn to surface + 1m clearance.
    FVector SpawnLoc = SpawnTransform.GetLocation();
    {
        FHitResult SurfHit;
        FCollisionQueryParams SurfParams(SCENE_QUERY_STAT(RaceGPSSpawnClearance), false);
        if (GetWorld()->LineTraceSingleByChannel(SurfHit,
                SpawnLoc + FVector(0, 0, 1000.0), SpawnLoc - FVector(0, 0, 200000.0),
                ECC_WorldStatic, SurfParams)
            && SurfHit.GetActor()
            && !SurfHit.GetActor()->IsA<APlayerStart>())
        {
            const float GroundZ = SurfHit.ImpactPoint.Z;
            if (GroundZ > SpawnLoc.Z)
            {
                UE_LOG(LogTemp, Log, TEXT("[raceGPS] Spawn clearance: spawn was %.1f uu under '%s'; lifted to Z=%.1f (ground %.1f + 100 clearance)"),
                    GroundZ - SpawnLoc.Z, *DiagActorName(SurfHit.GetActor()), GroundZ + 100.0f, GroundZ);
                SpawnLoc.Z = GroundZ + 100.0f;
            }
        }
    }

    APawn* ResultPawn = GetWorld()->SpawnActor<APawn>(GetDefaultPawnClassForController(NewPlayer),
        FTransform(SpawnTransform.GetRotation(), SpawnLoc, SpawnTransform.GetScale3D()), SpawnInfo);
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] SpawnDefaultPawn: %s at %s"),
        *GetNameSafe(ResultPawn), *SpawnLoc.ToString());
    return ResultPawn;
}

void ACruiseSprintGameMode::BindBakedCheckpointGates()
{
    if (bBakedGatesBound) return;
    bBakedGatesBound = true;
    if (LoadedRoutes.Num() == 0 || SelectedRouteIndex >= LoadedRoutes.Num()) return;

    const FAkronRouteSpline& Route = LoadedRoutes[SelectedRouteIndex];

    TArray<ACheckpointGate*> Gates;
    for (TActorIterator<ACheckpointGate> It(GetWorld()); It; ++It)
    {
        Gates.Add(*It);
    }

    // Match each loaded checkpoint to the nearest baked gate. Both derive from
    // the same compiler data (bake remaps (x,y,z)->(x,-z,y); GeoToWorld is now
    // the same Z-up convention), so correct matches land at ~0 distance.
    int32 Bound = 0;
    float MaxMatchDist = 0.0f;
    for (int32 i = 0; i < Route.CheckpointLocations.Num(); ++i)
    {
        const FVector Expected = UAkronXodrImporter::GeoToWorld(
            -Route.CheckpointLocations[i].Z, Route.CheckpointLocations[i].X, WorldOriginLat, WorldOriginLon);

        ACheckpointGate* Best = nullptr;
        float BestDist = MAX_FLT;
        for (ACheckpointGate* Gate : Gates)
        {
            const float D = FVector::Dist2D(Gate->GetActorLocation(), Expected);
            if (D < BestDist)
            {
                BestDist = D;
                Best = Gate;
            }
        }
        if (!Best || BestDist > 5000.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Baked map: no baked gate near checkpoint %d (best dist %.1f)"), i, BestDist);
            continue;
        }

        Best->CheckpointIndex = i;
        Best->ActivateGate();
        Best->OnCheckpointReached.AddDynamic(this, &ACruiseSprintGameMode::OnCheckpointReached);
        Gates.Remove(Best); // never bind one gate to two indices
        Bound++;
        MaxMatchDist = FMath::Max(MaxMatchDist, BestDist);
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Baked map: bound %d/%d checkpoint gates (world gates total: %d, max match dist %.1f); runtime-spawned gates: 0"),
        Bound, Route.CheckpointLocations.Num(), Gates.Num() + Bound, MaxMatchDist);
    DiagnosticsReportGates(Bound, Route.CheckpointLocations.Num());
}

void ACruiseSprintGameMode::SpawnGhostOnBakedRoute()
{
    if (LoadedRoutes.Num() == 0 || SelectedRouteIndex >= LoadedRoutes.Num()) return;

    const FAkronRouteSpline& Route = LoadedRoutes[SelectedRouteIndex];
    if (Route.Waypoints.Num() < 2) return;

    // Pick the baked route spline whose first point matches the selected
    // route's first waypoint (nearest-position match; labels are editor-only).
    const FVector ExpectedStart = UAkronXodrImporter::GeoToWorld(
        -Route.Waypoints[0].Z, Route.Waypoints[0].X, WorldOriginLat, WorldOriginLon);

    ARouteSplineActor* Best = nullptr;
    float BestDist = MAX_FLT;
    int32 SplineCount = 0;
    for (TActorIterator<ARouteSplineActor> It(GetWorld()); It; ++It)
    {
        ARouteSplineActor* Candidate = *It;
        if (!Candidate->Spline || Candidate->Spline->GetNumberOfSplinePoints() < 2) continue;
        SplineCount++;
        const float D = FVector::Dist2D(
            Candidate->Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World), ExpectedStart);
        if (D < BestDist)
        {
            BestDist = D;
            Best = Candidate;
        }
    }

    if (!Best || BestDist > 5000.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[raceGPS] Baked map: no baked route spline matched route %s (%d candidates); ghost disabled"),
            *Route.RouteId, SplineCount);
        return;
    }

    TArray<FVector> WorldWaypoints;
    const int32 NumPoints = Best->Spline->GetNumberOfSplinePoints();
    WorldWaypoints.Reserve(NumPoints);
    for (int32 i = 0; i < NumPoints; ++i)
    {
        WorldWaypoints.Add(Best->Spline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AGhostVehicle* Ghost = GetWorld()->SpawnActor<AGhostVehicle>(
        AGhostVehicle::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (Ghost)
    {
        Ghost->SetRouteWaypoints(WorldWaypoints);
        Ghost->StartGhostRun(CountdownDuration + 2.0f);
    }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Baked map: ghost follows baked route spline (%d pts, %d baked splines, match dist %.1f); runtime-spawned splines: 0"),
        NumPoints, SplineCount, BestDist);
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

// ============================================================================
// Runtime self-diagnostics (cvar: racegps.Diagnostics)
// ============================================================================

// Actor labels are editor-only; fall back to object names in packaged builds.
static FString DiagActorName(const AActor* Actor)
{
    if (!Actor) return TEXT("?");
#if WITH_EDITOR
    return Actor->GetActorLabel();
#else
    return Actor->GetName();
#endif
}

bool ACruiseSprintGameMode::IsDiagnosticsEnabled() const
{
    return CVarRaceGPSDiagnostics.GetValueOnGameThread() != 0;
}

void ACruiseSprintGameMode::RunDiagnosticsPreflight()
{
    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;

    // --- Lighting rig ---------------------------------------------------------
    int32 DirLights = 0, SkyLights = 0;
    for (TActorIterator<ADirectionalLight> It(World); It; ++It) { DirLights++; }
    for (TActorIterator<ASkyLight> It(World); It; ++It) { SkyLights++; }
    UE_LOG(LogTemp, Log, TEXT("[raceGPS-PREFLIGHT] %s lighting-rig (directional=%d, skylight=%d)"),
        (DirLights > 0 && SkyLights > 0) ? TEXT("PASS") : TEXT("FAIL"), DirLights, SkyLights);

    // --- Terrain collision: trace from spawn +1000uu straight down ------------
    FVector ProbeStart;
    if (Pawn)
    {
        ProbeStart = Pawn->GetActorLocation() + FVector(0, 0, 1000.0);
    }
    else
    {
        APlayerStart* PS = nullptr;
        for (TActorIterator<APlayerStart> It(World); It; ++It) { PS = *It; break; }
        ProbeStart = (PS ? PS->GetActorLocation() : FVector::ZeroVector) + FVector(0, 0, 1000.0);
    }
    FHitResult TerrainHit;
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(RaceGPSDiag), true);
    if (Pawn) { TraceParams.AddIgnoredActor(Pawn); }
    const bool bTerrainHit = World->LineTraceSingleByChannel(
        TerrainHit, ProbeStart, ProbeStart - FVector(0, 0, 200000.0), ECC_WorldStatic, TraceParams);
    const bool bTerrainNamed = bTerrainHit && TerrainHit.GetActor() &&
        (DiagActorName(TerrainHit.GetActor()).Contains(TEXT("Terrain")) ||
         TerrainHit.GetActor()->GetName().Contains(TEXT("Terrain")));
    UE_LOG(LogTemp, Log, TEXT("[raceGPS-PREFLIGHT] %s terrain-collision (down-trace from spawn+1000uu: %s%s)"),
        bTerrainHit ? TEXT("PASS") : TEXT("FAIL"),
        bTerrainHit ? *FString::Printf(TEXT("hit '%s'/'%s' dist=%.1f"),
            *DiagActorName(TerrainHit.GetActor()),
            TerrainHit.GetComponent() ? *TerrainHit.GetComponent()->GetName() : TEXT("?"),
            TerrainHit.Distance) : TEXT("NO HIT"),
        bTerrainHit && !bTerrainNamed ? TEXT(" [note: hit actor not named Terrain]") : TEXT(""));

    // --- Pawn spawn -----------------------------------------------------------
    UE_LOG(LogTemp, Log, TEXT("[raceGPS-PREFLIGHT] %s pawn-spawn (%s at %s)"),
        Pawn ? TEXT("PASS") : TEXT("FAIL"),
        Pawn ? *Pawn->GetName() : TEXT("<no pawn>"),
        Pawn ? *Pawn->GetActorLocation().ToString() : TEXT("n/a"));

    // --- GameInstance ---------------------------------------------------------
    const bool bGIOk = Cast<UraceGPSGameInstance>(GetGameInstance()) != nullptr;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS-PREFLIGHT] %s game-instance (class '%s')"),
        bGIOk ? TEXT("PASS") : TEXT("FAIL"),
        *GetNameSafe(GetGameInstance() ? GetGameInstance()->GetClass() : nullptr));

    // --- Vehicle tuning -------------------------------------------------------
    UE_LOG(LogTemp, Log, TEXT("[raceGPS-PREFLIGHT] %s vehicle-tuning (%s)"),
        SelectedVehicleTuning ? TEXT("PASS") : TEXT("FAIL"),
        SelectedVehicleTuning ? *SelectedVehicleTuning->DisplayName : TEXT("<none>"));

    // --- Gates: expected count; bound count reported once binding resolves ----
    int32 ExpectedGates = 0;
    if (LoadedRoutes.IsValidIndex(SelectedRouteIndex))
    {
        ExpectedGates = LoadedRoutes[SelectedRouteIndex].CheckpointLocations.Num();
    }
    int32 WorldGates = 0;
    for (TActorIterator<ACheckpointGate> It(World); It; ++It) { WorldGates++; }
    UE_LOG(LogTemp, Log, TEXT("[raceGPS-PREFLIGHT] %s gates-expected (route expects %d, world has %d; bound check follows at race start)"),
        ExpectedGates > 0 ? TEXT("PASS") : TEXT("FAIL"), ExpectedGates, WorldGates);
}

void ACruiseSprintGameMode::DiagnosticsReportGates(int32 BoundCount, int32 ExpectedCount)
{
    DiagGatesBound = BoundCount;
    if (!IsDiagnosticsEnabled()) return;
    UE_LOG(LogTemp, Log, TEXT("[raceGPS-PREFLIGHT] %s gates-bound (%d/%d bound)"),
        BoundCount == ExpectedCount ? TEXT("PASS") : TEXT("FAIL"), BoundCount, ExpectedCount);
}

void ACruiseSprintGameMode::DiagnosticsSampleTick()
{
    UWorld* World = GetWorld();
    if (!World) return;

    DiagSampleCount++;
    const float Now = World->GetTimeSeconds();

    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;

    FString PawnStr = TEXT("<no pawn>");
    FString VelStr = TEXT("n/a");
    FString DownStr = TEXT("n/a");
    FString UpStr = TEXT("n/a");
    FString OverlapStr = TEXT("n/a");
    bool bDownHit = false;
    float DownDist = 0.0f;
    bool bUpClose = false;
    bool bBuildingOverlap = false;
    float VelZ = 0.0f;

    if (Pawn)
    {
        const FVector Loc = Pawn->GetActorLocation();
        VelZ = Pawn->GetVelocity().Z;
        PawnStr = Loc.ToString();
        VelStr = FString::Printf(TEXT("%.1f"), VelZ);

        FCollisionQueryParams Params(SCENE_QUERY_STAT(RaceGPSDiag), true);
        Params.AddIgnoredActor(Pawn);
        FHitResult Hit;

        // Downward trace: ground under the pawn?
        if (World->LineTraceSingleByChannel(Hit, Loc + FVector(0, 0, 100.0), Loc - FVector(0, 0, 200000.0), ECC_WorldStatic, Params))
        {
            bDownHit = true;
            DownDist = Hit.Distance;
            DownStr = FString::Printf(TEXT("hit '%s'/'%s' dist=%.1f"),
                *DiagActorName(Hit.GetActor()),
                Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("?"),
                Hit.Distance);
        }
        else
        {
            DownStr = TEXT("NO HIT (void below)");
        }

        // Upward trace: ceiling right above = enclosed in a building?
        if (World->LineTraceSingleByChannel(Hit, Loc, Loc + FVector(0, 0, 50000.0), ECC_WorldStatic, Params))
        {
            bUpClose = Hit.Distance < 1500.0f;
            UpStr = FString::Printf(TEXT("hit '%s'/'%s' dist=%.1f"),
                *DiagActorName(Hit.GetActor()),
                Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("?"),
                Hit.Distance);
        }
        else
        {
            UpStr = TEXT("clear");
        }

        // Overlap test against building HISMs at pawn location.
        TArray<FOverlapResult> Overlaps;
        FCollisionObjectQueryParams ObjParams;
        ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
        World->OverlapMultiByObjectType(Overlaps, Loc, FQuat::Identity, ObjParams,
            FCollisionShape::MakeBox(FVector(300.0f)), Params);
        int32 BuildingHits = 0;
        FString FirstBuilding;
        for (const FOverlapResult& Ov : Overlaps)
        {
            UPrimitiveComponent* Comp = Ov.GetComponent();
            if (!Comp) continue;
            const bool bIsHISM = Comp->IsA<UHierarchicalInstancedStaticMeshComponent>();
            const FString CompName = Comp->GetName();
            if (bIsHISM)
            {
                BuildingHits++;
                bBuildingOverlap = true;
                if (FirstBuilding.IsEmpty())
                {
                    FirstBuilding = FString::Printf(TEXT("%s/%s"),
                        *DiagActorName(Ov.GetActor()), *CompName);
                }
            }
        }
        OverlapStr = BuildingHits > 0
            ? FString::Printf(TEXT("%d building HISM(s): %s"), BuildingHits, *FirstBuilding)
            : FString::Printf(TEXT("0 (of %d overlaps)"), Overlaps.Num());
    }

    FString CamStr = TEXT("<no cam>");
    if (PC && PC->PlayerCameraManager)
    {
        CamStr = PC->PlayerCameraManager->GetCameraLocation().ToString();
    }

    int32 WorldGates = 0;
    for (TActorIterator<ACheckpointGate> It(World); It; ++It) { WorldGates++; }

    UE_LOG(LogTemp, Log, TEXT("[raceGPS-DIAG] t=%.1fs pawn=%s velZ=%s | down: %s | up: %s | overlap: %s | cam=%s | gates bound=%d world=%d | fps=%.1f"),
        Now, *PawnStr, *VelStr, *DownStr, *UpStr, *OverlapStr, *CamStr,
        DiagGatesBound, WorldGates, 1.0f / FMath::Max(FApp::GetDeltaTime(), 0.0001f));

    // --- T+10s: screenshot (not under -nullrhi) + verdict ---------------------
    if (DiagSampleCount == 10 && !bDiagShotDone)
    {
        bDiagShotDone = true;
        if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
        {
            const FString ShotDir = FPaths::ScreenShotDir();
            UE_LOG(LogTemp, Log, TEXT("[raceGPS-DIAG] capturing HighResShot 1280x720 -> %s"), *ShotDir);
            if (GEngine)
            {
                GEngine->Exec(World, TEXT("HighResShot 1280x720"));
            }
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[raceGPS-DIAG] HighResShot skipped (-nullrhi)"));
        }

        FString Verdict;
        if (!Pawn)
        {
            Verdict = TEXT("NO_PAWN");
        }
        else if (VelZ < -100.0f || !bDownHit)
        {
            Verdict = TEXT("FALLING");
        }
        else if (bBuildingOverlap || bUpClose)
        {
            Verdict = TEXT("ENCLOSED");
        }
        else if (bDownHit && DownDist < 500.0f && FMath::Abs(VelZ) < 50.0f)
        {
            Verdict = TEXT("ON_GROUND");
        }
        else
        {
            Verdict = TEXT("AIRBORNE");
        }
        UE_LOG(LogTemp, Log, TEXT("[raceGPS-DIAG] VERDICT %s (pawn=%s velZ=%.1f down=%s gates=%d)"),
            *Verdict, *PawnStr, VelZ, *DownStr, DiagGatesBound);
    }

    // Cheap: stop after 15 samples.
    if (DiagSampleCount >= 15)
    {
        World->GetTimerManager().ClearTimer(DiagTimerHandle);
        UE_LOG(LogTemp, Log, TEXT("[raceGPS-DIAG] diagnostics ticker stopped after %d samples"), DiagSampleCount);
    }
}
