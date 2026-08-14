#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "Curves/CurveFloat.h"
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

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetThrottleInput(float Value);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetSteeringInput(float Value);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetBrakeInput(float Value);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void SetHandbrakeInput(bool bActive);

    UFUNCTION()
    void HandbrakePressed();

    UFUNCTION()
    void HandbrakeReleased();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void ResetVehicle();

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void ToggleCamera();

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
    float SteeringSensitivity = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    float ThrottleSensitivity = 1.0f;

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
    float LateralGripMultiplier = 1.8f;

    /** Fraction of tire force kept once the friction circle clips (maps to UChaosVehicleWheel::SideSlipModifier). Lower = longer, more controllable slides. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.3", UIMax = "1.0"))
    float DriftGripRetention = 0.6f;

    /** Vehicle drift angle (deg) beyond which DriftFrictionScale is applied to sustain a controlled power slide. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "0.0", UIMin = "5.0", UIMax = "45.0"))
    float DriftAngleThresholdDeg = 12.0f;

    /** Extra friction scale applied while drifting past DriftAngleThresholdDeg (smoothed). 1.0 = no drift-specific reduction. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.5", UIMax = "1.0"))
    float DriftFrictionScale = 0.85f;

    /** Maximum fractional grip reduction at SpeedGripReductionMaxSpeedKmh (0 = none). Keeps high-speed driving planted but honest. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Driving|Arcade", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.5"))
    float SpeedGripReductionFactor = 0.2f;

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

    // Arcade handling runtime state (S9)
    /** Authored per-wheel FrictionForceMultiplier captured before any arcade scaling (avoids compounding on re-apply). */
    TArray<float> BaseWheelFriction;
    /** Last friction value pushed to the physics sim per wheel (avoids spamming physics commands). */
    TArray<float> LastAppliedWheelFriction;
    /** Smoothed drift grip scale (1 = not drifting, DriftFrictionScale = drifting). */
    float CurrentDriftGripScale = 1.0f;

    void UpdateCameraView();
    void InitChaosVehicleMovement();
    void ApplyTuningData();
    void SetupWheel(class UChaosWheeledVehicleMovementComponent* WheeledComp, int32 WheelIndex, const struct FWheelTuning& Wheel);

    /** One-time application of static arcade tire params (grip scale, skid retention, ABS, steering curve). */
    void ApplyArcadeHandling(class UChaosWheeledVehicleMovementComponent* WheeledComp);
    /** Per-tick dynamic grip scaling (speed-sensitive reduction + drift friction scale). */
    void UpdateArcadeGrip(class UChaosWheeledVehicleMovementComponent* WheeledComp, float DeltaTime);
};
