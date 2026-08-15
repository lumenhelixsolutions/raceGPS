#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AkronXodrImporter.h"
#include "CruiseSprintGameMode.generated.h"

UENUM(BlueprintType)
enum class ECruiseSprintState : uint8
{
    None            UMETA(Hidden),
    Loading         UMETA(DisplayName = "Loading"),
    Countdown       UMETA(DisplayName = "Countdown"),
    Racing          UMETA(DisplayName = "Racing"),
    Finished        UMETA(DisplayName = "Finished"),
    Paused          UMETA(DisplayName = "Paused")
};

UCLASS()
class RACEGPSAKRONBETA_API ACruiseSprintGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACruiseSprintGameMode(const FObjectInitializer& ObjectInitializer);

    virtual void StartPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void StartRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void PauseRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void ResumeRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void FinishRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void RestartRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|GameMode")
    void StartRaceForAllPlayers();

    UFUNCTION()
    void OnCheckpointReached(int32 CheckpointIndex);

    UFUNCTION()
    void OnVehicleCollision(float ImpactSpeedKmh);

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    ECruiseSprintState GetRaceState() const { return CurrentState; }

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    float GetElapsedTime() const { return ElapsedTime; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    int32 GetCurrentCheckpoint() const { return CurrentCheckpoint; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    int32 GetTotalCheckpoints() const;

    UFUNCTION(BlueprintPure, Category = "raceGPS|GameMode")
    float GetTotalRaceDistance() const;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    FString CityPackPath = TEXT("../../citypacks/akron-oh-beta-001/");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    FString ManifestFile = TEXT("akron_semantic_manifest.json");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    FString XodrFile = TEXT("akron.xodr");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    FString RouteDir = TEXT("../../citypacks/akron-oh-beta-001/routes/");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    int32 SelectedRouteIndex = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    float CountdownDuration = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    float GoldTimeSeconds = 120.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    float SilverTimeSeconds = 150.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    float BronzeTimeSeconds = 200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    TSubclassOf<class UPauseMenuWidget> PauseMenuClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    TSubclassOf<class UMainMenuWidget> MainMenuClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    TSubclassOf<class UMinimapWidget> MinimapClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    TSubclassOf<class UCompassWidget> CompassClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    TSubclassOf<class UDeveloperConsole> DeveloperConsoleClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    TSubclassOf<class ULoadingScreenWidget> LoadingScreenClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    TSubclassOf<class UPostRaceStatsWidget> PostRaceStatsClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    TSubclassOf<class UTutorialWidget> TutorialWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|GameMode")
    TSubclassOf<class ANeonHUD> NeonHUDClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|World")
    TSubclassOf<class ABuildingMeshGenerator> BuildingGeneratorClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|World")
    TSubclassOf<class AStreetFurnitureSpawner> FurnitureSpawnerClass;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    TArray<FAkronRouteSpline> LoadedRoutes;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    TArray<FAkronSpawnPoint> LoadedSpawns;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    TArray<FAkronPOI> LoadedPOIs;

    /** Resolved layout of the active citypack (populated in StartPlay from config/cvar/cmdline). */
    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|City")
    FRaceGPSCityLayout CityLayout;

    UPROPERTY()
    TObjectPtr<class UPauseMenuWidget> ActivePauseMenu;

    UPROPERTY()
    TObjectPtr<class UMinimapWidget> MinimapWidget;

    UPROPERTY()
    TObjectPtr<class UCompassWidget> CompassWidget;

    UPROPERTY()
    TObjectPtr<class UDeveloperConsole> DevConsole;

    UPROPERTY()
    TObjectPtr<class ULoadingScreenWidget> LoadingScreen;

    UPROPERTY()
    TObjectPtr<class URaceScoringSystem> ScoringSystem;

    UPROPERTY()
    TObjectPtr<class URaceReplayManager> ReplayManager;

    UPROPERTY()
    TObjectPtr<class ULeaderboardSystem> LeaderboardSystem;

    UPROPERTY()
    TObjectPtr<class UTutorialSystem> TutorialSystem;

    UPROPERTY()
    TObjectPtr<class UAchievementSystem> AchievementSystem;

    UPROPERTY()
    TObjectPtr<class UConsoleCommands> ConsoleCommands;

    UPROPERTY()
    TObjectPtr<class AGhostVehicle> BestGhost;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    TArray<TObjectPtr<class UVehicleTuningData>> VehiclePresets;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Vehicle")
    TMap<FString, TObjectPtr<class UVehicleTuningData>> HandlingModePresets;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Vehicle")
    TObjectPtr<class UVehicleTuningData> SelectedVehicleTuning;

protected:
    virtual void OnRaceStateChanged(ECruiseSprintState NewState);

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    ECruiseSprintState CurrentState = ECruiseSprintState::None;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    float ElapsedTime = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    int32 CurrentCheckpoint = 0;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|GameMode")
    float CountdownTimer = 0.0f;

    float WorldOriginLat = 41.08f;
    float WorldOriginLon = -81.52f;

    void LoadCityData();
    bool IsVersionCompatible(const FString& CityVersion) const;
    void SpawnPlayerAtStart();
    void SpawnRouteSpline();
    void SpawnCheckpoints();

    /**
     * True when the loaded level IS the baked city map for the active city
     * (persistent-level package short name == CityLayout.LevelName). The baked
     * map already contains PlayerStarts, route splines and checkpoint gates at
     * correct baked (UE Z-up) positions, so runtime spawning/teleporting must
     * stand down or every gate/spline is duplicated (black-screen hotfix).
     */
    bool bRunningBakedCityMap = false;
    bool bBakedGatesBound = false;

    /** Baked path: index/activate/bind the map's existing checkpoint gates by
     *  nearest-position match against the selected route's checkpoints. */
    void BindBakedCheckpointGates();
    /** Baked path: spawn the ghost on the baked route spline's world points. */
    void SpawnGhostOnBakedRoute();

    /**
     * Stock GameModeBase spawns the default pawn with dont-spawn-if-colliding
     * semantics. The hero Charger's physics hull overlaps the runtime road
     * meshes at baked PlayerStarts, which failed the spawn outright (no pawn,
     * dead camera). Force adjust-or-always-spawn so the hero always spawns.
     */
    virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
    void UpdateCountdown(float DeltaTime);

    // --- Runtime self-diagnostics (permanent feature) -------------------------
    // Gated by cvar "racegps.Diagnostics" (default ON in Development builds,
    // settable via [SystemSettings] in DefaultEngine.ini or -racegps.Diagnostics=1).
    // Emits greppable [raceGPS-PREFLIGHT] PASS/FAIL lines at StartPlay, then one
    // structured [raceGPS-DIAG] sample per second for 15 s (traces/overlaps/FPS),
    // a HighResShot at T+10s (skipped under -nullrhi) and a final VERDICT line.
    // Zero cost after the ticker stops; no per-frame work.
    bool IsDiagnosticsEnabled() const;
    void RunDiagnosticsPreflight();
    void DiagnosticsSampleTick();
    /** Emits "[raceGPS-PREFLIGHT] PASS/FAIL gates-bound ..." once gates resolve. */
    void DiagnosticsReportGates(int32 BoundCount, int32 ExpectedCount);

    FTimerHandle DiagTimerHandle;
    int32 DiagSampleCount = 0;
    bool bDiagShotDone = false;
    /** Gates bound for the active route; -1 = not resolved yet. */
    int32 DiagGatesBound = -1;

    void InitHUDWidgets();
    void CreateDefaultVehiclePresets();
    void LoadHandlingModePresets();
    TObjectPtr<class UVehicleTuningData> BuildMergedVehicleTuning(class UVehicleTuningData* BaseVehiclePreset, const FString& HandlingMode);
    void ApplyVehicleTuningToPlayer();
};
