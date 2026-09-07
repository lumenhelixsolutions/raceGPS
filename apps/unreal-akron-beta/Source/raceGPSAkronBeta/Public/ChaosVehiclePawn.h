#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Curves/CurveFloat.h"
#include "ClevelandShowcaseTypes.h"
#include "ChaosVehiclePawn.generated.h"

class UVehicleTuningData;

UCLASS()
class RACEGPSAKRONBETA_API AChaosVehiclePawn : public AWheeledVehiclePawn
{
    GENERATED_BODY()

public:
    AChaosVehiclePawn(const FObjectInitializer& ObjectInitializer);

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents() override;

    /** CARLA Charger body. Hellcat is a look+tune, not a licensed Dodge mesh. */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle|Look")
    void ApplyVehicleLook(EVehicleLook Look);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle|Look")
    void EnsureCarlaChargerMesh();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle|Look")
    void ApplyHellcatTune();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle|Look")
    EVehicleLook VehicleLook = EVehicleLook::Hellcat;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "raceGPS|Vehicle|Look")
    FLinearColor BodyTint = FLinearColor(1.0f, 0.28f, 0.05f, 1.0f);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetThrottleInput(float Value);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetSteeringInput(float Value);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetBrakeInput(float Value);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetHandbrakeInput(bool bActive);

    /** AI / auto-drive writes these; player axis is ignored while override is set. */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetDriveOverride(float Steer, float Throttle, float Brake, bool bInHandbrake);
    /** Clear hold brake/handbrake (works even with drive override) and force gear 1. */
    void ReleaseForRace();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void ClearDriveOverride();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void WakeForDrive();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void CloseVehicleDoors();

    void DumpDriveState(const TCHAR* Tag);

    UFUNCTION(BlueprintPure, Category = "raceGPS|Vehicle")
    bool IsDriveOverrideActive() const { return bDriveOverride; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Vehicle")
    bool IsHandbrakeOn() const { return bHandbrake; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Vehicle")
    float GetThrottleCommand() const { return CurrentThrottle; }

    UFUNCTION(BlueprintPure, Category = "raceGPS|Vehicle")
    float GetBrakeCommand() const { return CurrentBrake; }

    UFUNCTION()
    void HandbrakePressed();

    UFUNCTION()
    void HandbrakeReleased();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void ResetVehicle();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void ToggleCamera();

    /**
     * Cleveland Historic Circuit chase (V7).
     * World: X=east, Y=north. Downtown HISM (~120k) is SOUTH; Lake Erie is NORTH.
     * World-locks the spring arm to look WSW (city left / lake right) so a pawn that
     * faces along the runway cannot aim the chase north into empty water.
     * Does not change Akron/CruiseSprint constructor defaults.
     */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Camera")
    void ApplyClevelandShowcaseChaseFraming();

    /** Night headlight / taillight emissive pools for Cleveland showcase stills. */
    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle|Look")
    void EnsureShowcaseNightLights();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    float GetSpeedKmh() const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    float GetEngineRPM() const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    int32 GetCurrentGear() const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetTuningData(UVehicleTuningData* NewTuningData);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    float CalculateDriftAngle() const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    float CalculateWheelSlipRatio() const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    TObjectPtr<UVehicleTuningData> TuningData;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    float MaxSpeedKmh = 200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    float SteeringSensitivity = 1.15f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    float ThrottleSensitivity = 1.45f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Camera")
    TArray<FTransform> CameraTransforms;

    // ------------------------------------------------------------------
    // Arcade handling ("Midnight Club" feel) — S9
    //
    // All effects are multiplicative against each wheel class's authored
    // Chaos values (UChaosVehicleWheel::FrictionForceMultiplier etc.),
    // which are captured once before any modification. Therefore:
    //   bEnableArcadeHandling = false            -> 100% stock behavior
    //   LateralGripMultiplier = 1.0,
    //   DriftGripRetention    = 1.0,
    //   DriftFrictionScale    = 1.0,
    //   SpeedGripReductionFactor = 0.0           -> 100% stock behavior
    // The shipped defaults below dial in the arcade feel: ~1.8x lateral
    // grip, controlled power slides past DriftAngleThresholdDeg, and a
    // gentle high-speed grip reduction.
    // ------------------------------------------------------------------

    /** Master switch for all arcade tire/handling effects. When false nothing is touched. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade")
    bool bEnableArcadeHandling = true;

    /** Scales each wheel's authored FrictionForceMultiplier (overall grip; lateral grip is friction-circle clamped to this). 1.8 = arcade high grip. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "3.0"))
    float LateralGripMultiplier = 2.0f;

    /** Fraction of tire force kept once the friction circle clips (maps to UChaosVehicleWheel::SideSlipModifier). Lower = longer, more controllable slides. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.3", UIMax = "1.0"))
    float DriftGripRetention = 0.75f;

    /** Vehicle drift angle (deg) beyond which DriftFrictionScale is applied to sustain a controlled power slide. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "0.0", UIMin = "5.0", UIMax = "45.0"))
    float DriftAngleThresholdDeg = 12.0f;

    /** Extra friction scale applied while drifting past DriftAngleThresholdDeg (smoothed). 1.0 = no drift-specific reduction. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.5", UIMax = "1.0"))
    float DriftFrictionScale = 0.92f;

    /** Maximum fractional grip reduction at SpeedGripReductionMaxSpeedKmh (0 = none). Keeps high-speed driving planted but honest. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.5"))
    float SpeedGripReductionFactor = 0.08f;

    /** Speed (km/h) at which SpeedGripReductionFactor is fully applied. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "1.0", UIMin = "100.0", UIMax = "400.0"))
    float SpeedGripReductionMaxSpeedKmh = 220.0f;

    /** Enable Chaos per-wheel ABS (arcade brake assist). Note: Chaos wheel default is OFF, so this changes braking even at neutral multipliers. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade")
    bool bEnableABS = true;

    /** Replace the movement component's speed-sensitive steering curve (X = MPH, Y = fraction of max steer). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade")
    bool bUseArcadeSteeringCurve = true;

    /** Speed-sensitive steering curve used when bUseArcadeSteeringCurve is set. X = forward speed in MPH, Y = steering scale (keep max key value at 1.0). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (EditCondition = "bUseArcadeSteeringCurve"))
    FRuntimeFloatCurve ArcadeSteeringCurve;

    UFUNCTION(BlueprintPure, Category = "raceGPS|Vehicle")
    class UVehicleAudioComponent* GetAudioComponent() const { return AudioComponent; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Components")
    TObjectPtr<class USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Components")
    TObjectPtr<class UCameraComponent> ChaseCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Components")
    TObjectPtr<class UCameraComponent> HoodCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Components")
    TObjectPtr<class UArrowComponent> Arrow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "raceGPS|Components")
    TObjectPtr<class UVehicleAudioComponent> AudioComponent;

    UFUNCTION()
    void OnVehicleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                      UPrimitiveComponent* OtherComp, FVector NormalImpulse,
                      const FHitResult& Hit);

private:
    float CurrentThrottle = 0.0f;
    float CurrentSteering = 0.0f;
    float CurrentBrake = 0.0f;
    bool bHandbrake = false;
    int32 ActiveCameraIndex = 0;

    bool bDriveOverride = false;
    float OverrideThrottle = 0.0f;
    float OverrideSteering = 0.0f;
    float OverrideBrake = 0.0f;
    bool bOverrideHandbrake = false;

    // Arcade handling runtime state (S9)
    /** Authored per-wheel FrictionForceMultiplier captured before any arcade scaling (avoids compounding on re-apply). */
    TArray<float> BaseWheelFriction;
    /** Last friction value pushed to the physics sim per wheel (avoids spamming physics commands). */
    TArray<float> LastAppliedWheelFriction;
    /** Smoothed drift grip scale (1 = not drifting, DriftFrictionScale = drifting). */
    float CurrentDriftGripScale = 1.0f;

    void UpdateCameraView();
    void BoostNightPaintEmissive(UMaterialInstanceDynamic* MID, EVehicleLook Look) const;

    UPROPERTY()
    TObjectPtr<class UPointLightComponent> HeadlightL;

    UPROPERTY()
    TObjectPtr<class UPointLightComponent> HeadlightR;

    UPROPERTY()
    TObjectPtr<class UPointLightComponent> TaillightL;

    UPROPERTY()
    TObjectPtr<class UPointLightComponent> TaillightR;
    void UpdateClevelandShowcaseChaseFraming();
    bool bClevelandShowcaseChaseFraming = false;
    void InitChaosVehicleMovement();
    /** Fill torque curve / mech / gears BEFORE Chaos CreateVehicle (empty curve disables mech). */
    void EnsureEngineDriveConfig();
    /** MoveComp ticks after pawn throttle writes; re-assert after every RecreatePhysicsState (PrePhysics; no AddTickPrerequisite cycle). */
    void AssertMoveCompTickAfterPawn();
    void EnsureDefaultWheels();
    bool bChaosSimReady = false;
    bool bLoggedAuthoredWheels = false;
    bool bAuthoredWheelsTorqueFixed = false;
    /** One-shot wheel CDO/physics rebuild — shared WheelClass CDOs thrash AxleType Front/Rear if rebuilt every tick. */
    bool bDriveWheelsPatched = false;
    void ApplyTuningData();
    void SetupWheel(class UChaosWheeledVehicleMovementComponent* WheeledComp, int32 WheelIndex, const struct FWheelTuning& Wheel);

    /** One-time application of static arcade tire params (grip scale, skid retention, ABS, steering curve). */
    void ApplyArcadeHandling(class UChaosWheeledVehicleMovementComponent* WheeledComp);
    /** Per-tick dynamic grip scaling (speed-sensitive reduction + drift friction scale). */
    void UpdateArcadeGrip(class UChaosWheeledVehicleMovementComponent* WheeledComp, float DeltaTime);
};

