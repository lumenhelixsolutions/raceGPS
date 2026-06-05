#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    TObjectPtr<UVehicleTuningData> TuningData;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    float MaxSpeedKmh = 200.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    float SteeringSensitivity = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Vehicle")
    float ThrottleSensitivity = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|Camera")
    TArray<FTransform> CameraTransforms;

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

    void UpdateCameraView();
    void InitChaosVehicleMovement();
    void ApplyTuningData();
    void SetupWheel(int32 WheelIndex, const struct FWheelTuning& Wheel);
};
