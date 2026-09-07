#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "ClevelandLookDirector.generated.h"

UENUM(BlueprintType)
enum class EClevelandVisualMode : uint8
{
    SunnyDay UMETA(DisplayName = "Sunny Day"),
    MidnightRun UMETA(DisplayName = "Midnight Run")
};

/**
 * Visual sprint director: sunny volumetric-cloud day, or Midnight Club night look.
 * Uses existing ADayNightCycle + APostProcessController. No CARLA server.
 *
 * Default is MidnightRun: the Cleveland showcase is graded as a night street-race,
 * not a noon tech-demo. SunnyDay remains available via ApplyVisualMode.
 */
UCLASS()
class RACEGPSAKRONBETA_API AClevelandLookDirector : public AActor
{
    GENERATED_BODY()

public:
    AClevelandLookDirector();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Cleveland|Look")
    EClevelandVisualMode Mode = EClevelandVisualMode::MidnightRun;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Cleveland|Look")
    void ApplyVisualMode(EClevelandVisualMode InMode);

protected:
    virtual void BeginPlay() override;

    void ApplySunnyDay();
    void ApplyMidnightRun();
    void EnsureCycleAndPost();
    void SuppressCompetingLights() const;
    void ApplyEpicConsoleVars() const;
    void ApplyLookToEnvironment() const;
    void EnsureLightingFailsafe() const;
    void EnsureNightFogAndLamps();
    void ApplyNightSkyFix() const;
    void ApplyNightCityHISMGlow() const;
    void ApplyNightGroundWetness() const;
    void HideSprawlBuildingHISM();
    void QuietCesiumTilesets() const;
    void EnsureBurkeWetApron();
    void LogFinalLook(const TCHAR* Tag) const;

    UPROPERTY()
    TObjectPtr<class ADayNightCycle> Cycle;

    UPROPERTY()
    TObjectPtr<class APostProcessController> Post;

    UPROPERTY()
    TObjectPtr<class AExponentialHeightFog> NightFog;

    UPROPERTY()
    TArray<TObjectPtr<class APointLight>> NightLamps;

    /** Runtime-only wet apron under Burke. Not saved. */
    UPROPERTY()
    TObjectPtr<class AStaticMeshActor> BurkeWetApron;

    /** Runtime-only south downtown HISM band. Original T10 HISMs stay on disk, hidden. */
    UPROPERTY()
    TObjectPtr<AActor> DowntownBandActor;

    bool bSprawlHidden = false;
};
