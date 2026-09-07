#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ClevelandShowcaseGameMode.generated.h"

class ARaceGridManager;
class URaceSessionManager;
class URacingLineComponent;
class AClevelandEnvironmentActor;
class AClevelandLookDirector;
class ACameraActor;
class AChaosVehiclePawn;
class ACheckpointGate;

/**
 * Cleveland Historic Circuit showcase. Extends the existing CruiseSprint / race
 * flow by composing URaceSessionManager (Menu -> Countdown -> Racing -> Finished)
 * plus a parallel grid/AI layer. Does not require the garage.
 *
 * Select this GameMode via World Settings or DefaultEngine.ini
 * (see CLEVELAND_CPP_WIREUP.md). Do not change GlobalDefaultGameMode away from CruiseSprint.
 *
 * V1 framing: a short intro camera sits EAST of the Burke grid and looks WSW so
 * downtown (south, ~120k T10 HISM buildings + Karla silhouette) is left-of-frame
 * and Lake Erie (north) is right-of-frame, then blends to a raised 3/4 chase cam.
 */
UCLASS()
class RACEGPSAKRONBETA_API AClevelandShowcaseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AClevelandShowcaseGameMode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland")
	FString ProductTitle = TEXT("raceGPS");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland")
	FString CircuitTitle = TEXT("CLEVELAND HISTORIC CIRCUIT");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland")
	FString CityPackRelativeDir = TEXT("citypacks/cleveland/burke_gp_1997");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland")
	int32 TotalCheckpoints = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland")
	TObjectPtr<URaceSessionManager> SessionManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland")
	TObjectPtr<ARaceGridManager> GridManager;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland")
	TObjectPtr<AClevelandEnvironmentActor> EnvironmentActor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Cleveland")
	TObjectPtr<AClevelandLookDirector> LookDirector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Camera")
	float IntroHoldSeconds = 7.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Camera")
	float IntroBlendSeconds = 1.6f;

	/** After intro blend completes, wait this long then capture chase still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Camera")
	float HeroCaptureDelaySeconds = 1.4f;

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland")
	void LoadCityPack();

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland")
	void BeginCountdownAndRace();

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland")
	void RestartShowcase();

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland")
	void EndRace();

	UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland")
	FString GetHudTitleLine() const;

	UFUNCTION()
	void OnShowcaseCheckpointReached(int32 CheckpointIndex);

	UFUNCTION(Exec)
	void ClevelandAutoLap();

	UFUNCTION(Exec)
	void ClevelandSkipCountdown();

	UFUNCTION(Exec)
	void ClevelandForceFinish();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void BindHud();
	int32 LoadCheckpointCount() const;
	FString ResolveCityPackPath(const FString& FileName) const;
	void StartSkylineIntro(APlayerController* PC);
	void FinishSkylineIntro();
	void CaptureHeroStill();
	void CaptureStill(const TCHAR* Phase);
	void ApplyPlaytestFlags();
	void LoadCheckpointCourse();
	void SpawnShowcaseCheckpoints();
	void ConfigureAndStartRace();
	void TickPlayerCheckpoints(float DeltaSeconds);
	float CanonicalizeSplineS(float S, float Length) const;
	void WritePlaytestReport(const TCHAR* Outcome);
	void DumpDriveWhyNotMoving(const TCHAR* Reason);

	bool bShowcaseEnded = false;
	bool bIntroActive = false;
	bool bChaseCapturePending = false;
	bool bIntroCaptureDone = false;
	bool bChaseCaptureDone = false;
	float IntroElapsed = 0.f;
	float HeroCaptureElapsed = 0.f;

	UPROPERTY()
	TObjectPtr<ACameraActor> IntroCamera;

	TArray<float> CheckpointSCm;
	TArray<FString> CheckpointNames;
	int32 NextCheckpointIndex = 1;
	float PlayerCheckpointPrevS = 0.f;
	bool bHaveCheckpointPrevS = false;
	bool bAutoDrivePlayer = false;
	bool bSkipIntro = false;
	bool bPlaytestLap = false;
	float PlaytestLogElapsed = 0.f;
	TArray<FString> PlaytestCheckpointLines;
	float RacingZeroSpeedSeconds = 0.f;
	bool bDumpedDriveDiag = false;
	bool bPlaytestStillCaptured = false;
	bool bSawPositiveSpeed = false;
	bool bStuckQuitIssued = false;

	UPROPERTY()
	TArray<TObjectPtr<ACheckpointGate>> ShowcaseGates;
};
