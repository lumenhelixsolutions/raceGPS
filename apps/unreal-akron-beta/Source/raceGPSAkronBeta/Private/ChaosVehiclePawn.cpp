#include "ChaosVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/ArrowComponent.h"
#include "VehicleTuningData.h"
#include "VehicleAudioComponent.h"
#include "NeonHUD.h"
#include "CruiseSprintGameMode.h"

static EVehicleDifferential ToEngineDifferential(ERaceGPSDifferentialType Type)
{
    switch (Type)
    {
    case ERaceGPSDifferentialType::FrontWheelDrive: return EVehicleDifferential::FrontWheelDrive;
    case ERaceGPSDifferentialType::RearWheelDrive:  return EVehicleDifferential::RearWheelDrive;
    default:                                          return EVehicleDifferential::AllWheelDrive;
    }
}

AChaosVehiclePawn::AChaosVehiclePawn(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;

    // Spring arm for chase camera
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GetMesh());
    SpringArm->TargetArmLength = 450.0f;
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bInheritPitch = false;
    SpringArm->bInheritRoll = false;
    SpringArm->bInheritYaw = true;
    SpringArm->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
    SpringArm->SocketOffset = FVector(0.0f, 0.0f, 120.0f);

    // Chase camera
    ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
    ChaseCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    ChaseCamera->bUsePawnControlRotation = false;

    // Hood camera
    HoodCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("HoodCamera"));
    HoodCamera->SetupAttachment(GetMesh());
    HoodCamera->SetRelativeLocation(FVector(120.0f, 0.0f, 110.0f));
    HoodCamera->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
    HoodCamera->bUsePawnControlRotation = false;
    HoodCamera->SetAutoActivate(false);

    // Arrow for forward direction
    Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ForwardArrow"));
    Arrow->SetupAttachment(GetMesh());
    Arrow->SetRelativeLocation(FVector(180.0f, 0.0f, 50.0f));

    // Audio
    AudioComponent = CreateDefaultSubobject<UVehicleAudioComponent>(TEXT("AudioComponent"));

    // Default vehicle mesh placeholder
    // TODO: Import a real sedan skeletal mesh to /Game/Vehicles/Sedan/Sedan_SkelMesh
    // (or point to your car-kit output). The finder below is commented to allow cooking
    // while art/content is missing. The vehicle will have no visible mesh until fixed.
    // static ConstructorHelpers::FObjectFinder<USkeletalMesh> VehicleMesh(TEXT("/Game/Vehicles/Sedan/Sedan_SkelMesh.Sedan_SkelMesh"));
    // if (VehicleMesh.Succeeded())
    // {
    //     GetMesh()->SetSkeletalMesh(VehicleMesh.Object);
    // }
}

void AChaosVehiclePawn::BeginPlay()
{
    Super::BeginPlay();
    InitChaosVehicleMovement();
    ApplyTuningData();
    UpdateCameraView();

    GetMesh()->OnComponentHit.AddDynamic(this, &AChaosVehiclePawn::OnVehicleHit);
}

void AChaosVehiclePawn::InitChaosVehicleMovement()
{
    auto* MoveComp = GetVehicleMovementComponent();
    if (!MoveComp) return;

    // Ensure we have a proper physics setup for Chaos Vehicles
    MoveComp->StopMovementImmediately();

    // Basic engine torque curve (lives on the wheeled movement component in UE 5.7)
    if (auto* WheeledComp = Cast<UChaosWheeledVehicleMovementComponent>(MoveComp))
    {
        WheeledComp->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
        WheeledComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.0f, 400.0f);
        WheeledComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(2000.0f, 500.0f);
        WheeledComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(4000.0f, 450.0f);
        WheeledComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(6000.0f, 300.0f);
    }
}

void AChaosVehiclePawn::SetTuningData(UVehicleTuningData* NewTuningData)
{
    if (!NewTuningData)
        return;

    TuningData = NewTuningData;
    ApplyTuningData();
    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Tuning applied: %s"), *TuningData->DisplayName);
}

void AChaosVehiclePawn::ApplyTuningData()
{
    auto* MoveComp = GetVehicleMovementComponent();
    if (!MoveComp || !TuningData)
        return;

    // Vehicle mass and aero (base movement component)
    MoveComp->Mass = TuningData->VehicleMass;
    MoveComp->DragCoefficient = TuningData->DragCoefficient;
    MoveComp->ChassisWidth = TuningData->ChassisWidth;
    MoveComp->ChassisHeight = TuningData->ChassisHeight;
    MoveComp->DownforceCoefficient = TuningData->DownForceCoefficient;

    // Engine, transmission, differential and steering are on the wheeled movement component in UE 5.7
    auto* WheeledComp = Cast<UChaosWheeledVehicleMovementComponent>(MoveComp);
    if (!WheeledComp)
        return;

    // Engine
    WheeledComp->EngineSetup.MaxRPM = TuningData->MaxEngineRPM;
    WheeledComp->EngineSetup.EngineIdleRPM = TuningData->IdleRPM;
    WheeledComp->EngineSetup.EngineBrakeEffect = TuningData->BrakeTorque / 10000.0f;
    WheeledComp->EngineSetup.MaxTorque = 500.0f;

    // Steering
    WheeledComp->SteeringSetup.SteeringType = ESteeringType::Ackermann;

    // Transmission
    WheeledComp->TransmissionSetup.FinalRatio = TuningData->Transmission.FinalDriveRatio;
    WheeledComp->TransmissionSetup.ForwardGearRatios = TuningData->Transmission.GearRatios;
    WheeledComp->TransmissionSetup.ReverseGearRatios.Empty();
    WheeledComp->TransmissionSetup.ReverseGearRatios.Add(TuningData->Transmission.ReverseGearRatio);
    WheeledComp->TransmissionSetup.ChangeUpRPM = TuningData->Transmission.UpShiftRPM;
    WheeledComp->TransmissionSetup.ChangeDownRPM = TuningData->Transmission.DownShiftRPM;
    WheeledComp->TransmissionSetup.GearChangeTime = (TuningData->Transmission.ChangeUpTime + TuningData->Transmission.ChangeDownTime) * 0.5f;

    // Differential
    WheeledComp->DifferentialSetup.DifferentialType = ToEngineDifferential(TuningData->Differential.DifferentialType);
    WheeledComp->DifferentialSetup.FrontRearSplit = TuningData->Differential.FrontRearSplit;

    // Wheels
    for (int32 i = 0; i < TuningData->Wheels.Num() && i < WheeledComp->WheelSetups.Num(); ++i)
    {
        SetupWheel(WheeledComp, i, TuningData->Wheels[i]);
    }
}

void AChaosVehiclePawn::SetupWheel(UChaosWheeledVehicleMovementComponent* WheeledComp, int32 WheelIndex, const FWheelTuning& Wheel)
{
    if (!WheeledComp || WheelIndex >= WheeledComp->WheelSetups.Num())
        return;

    // In UE 5.7 wheel shape configuration lives on the UChaosVehicleWheel class default object.
    FChaosWheelSetup& Setup = WheeledComp->WheelSetups[WheelIndex];
    UClass* WheelClass = Setup.WheelClass ? static_cast<UClass*>(Setup.WheelClass) : UChaosVehicleWheel::StaticClass();
    UChaosVehicleWheel* WheelCDO = Cast<UChaosVehicleWheel>(WheelClass->GetDefaultObject(true));
    if (!WheelCDO)
    {
        return;
    }

    WheelCDO->WheelRadius = Wheel.Radius;
    WheelCDO->WheelWidth = Wheel.Width;
    WheelCDO->WheelMass = Wheel.Mass;
    WheelCDO->MaxSteerAngle = Wheel.SteerAngle;
    WheelCDO->bAffectedBySteering = Wheel.SteerAngle != 0.0f;
    WheelCDO->bAffectedByEngine = Wheel.bDrive;
    WheelCDO->bAffectedByHandbrake = Wheel.bHandbrake;

    // Suspension
    WheelCDO->SpringRate = Wheel.SuspensionStiffness;
    WheelCDO->SuspensionDampingRatio = Wheel.SuspensionDamping;
    WheelCDO->SuspensionMaxRaise = Wheel.MaxRaise;
    WheelCDO->SuspensionMaxDrop = Wheel.MaxDrop;
    WheelCDO->SuspensionForceOffset = FVector(0.0f, 0.0f, Wheel.SuspensionForceOffset);
}

void AChaosVehiclePawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    auto* MoveComp = GetVehicleMovementComponent();
    if (MoveComp)
    {
        float FinalThrottle = CurrentThrottle * ThrottleSensitivity;
        float FinalSteering = CurrentSteering * SteeringSensitivity;

        // Drift assist: apply counter-steer when handbrake is active and sliding
        if (bHandbrake && TuningData)
        {
            float DriftAngle = CalculateDriftAngle();
            if (FMath::Abs(DriftAngle) > 5.0f)
            {
                float CounterSteer = -FMath::Sign(DriftAngle) * TuningData->CounterSteerGain * FMath::Clamp(FMath::Abs(DriftAngle) / TuningData->DriftAngleMax, 0.0f, 1.0f);
                FinalSteering = FMath::Lerp(FinalSteering, CounterSteer, 0.3f);
            }

        }

        // Traction control: reduce throttle when wheel slip is high
        if (TuningData && TuningData->TractionControl > 0.0f)
        {
            float SlipRatio = CalculateWheelSlipRatio();
            if (SlipRatio > 0.25f)
            {
                float TCTarget = FMath::Lerp(1.0f, 0.7f, TuningData->TractionControl);
                FinalThrottle *= FMath::Lerp(TCTarget, 1.0f, FMath::Clamp((SlipRatio - 0.25f) / 0.25f, 0.0f, 1.0f));
            }
        }

        MoveComp->SetThrottleInput(FinalThrottle);
        MoveComp->SetSteeringInput(FinalSteering);
        MoveComp->SetBrakeInput(CurrentBrake);
        MoveComp->SetHandbrakeInput(bHandbrake);
    }

    // Update audio with brake state
    if (AudioComponent)
    {
        AudioComponent->SetBrakeInput(CurrentBrake);
    }

    // Update HUD telemetry
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ANeonHUD* HUD = Cast<ANeonHUD>(PC->GetHUD()))
        {
            HUD->SetSpeedKmh(GetSpeedKmh());
            HUD->SetTelemetry(GetEngineRPM(), GetCurrentGear());
        }
    }
}

void AChaosVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis(TEXT("Throttle"), this, &AChaosVehiclePawn::SetThrottleInput);
    PlayerInputComponent->BindAxis(TEXT("Steer"), this, &AChaosVehiclePawn::SetSteeringInput);
    PlayerInputComponent->BindAxis(TEXT("Brake"), this, &AChaosVehiclePawn::SetBrakeInput);
    PlayerInputComponent->BindAction(TEXT("Handbrake"), IE_Pressed, this, &AChaosVehiclePawn::HandbrakePressed);
    PlayerInputComponent->BindAction(TEXT("Handbrake"), IE_Released, this, &AChaosVehiclePawn::HandbrakeReleased);
    PlayerInputComponent->BindAction(TEXT("ResetVehicle"), IE_Pressed, this, &AChaosVehiclePawn::ResetVehicle);
    PlayerInputComponent->BindAction(TEXT("ToggleCamera"), IE_Pressed, this, &AChaosVehiclePawn::ToggleCamera);
}

void AChaosVehiclePawn::SetThrottleInput(float Value)
{
    CurrentThrottle = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AChaosVehiclePawn::SetSteeringInput(float Value)
{
    CurrentSteering = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AChaosVehiclePawn::SetBrakeInput(float Value)
{
    CurrentBrake = FMath::Clamp(Value, 0.0f, 1.0f);
}

void AChaosVehiclePawn::SetHandbrakeInput(bool bActive)
{
    bHandbrake = bActive;
}

void AChaosVehiclePawn::HandbrakePressed()
{
    bHandbrake = true;
}

void AChaosVehiclePawn::HandbrakeReleased()
{
    bHandbrake = false;
}

void AChaosVehiclePawn::ResetVehicle()
{
    auto* MoveComp = GetVehicleMovementComponent();
    if (MoveComp)
    {
        MoveComp->StopMovementImmediately();
    }

    FVector CurrentLocation = GetActorLocation();
    FRotator CurrentRotation = GetActorRotation();
    CurrentLocation.Z += 50.0f;
    SetActorLocationAndRotation(CurrentLocation, CurrentRotation, false, nullptr, ETeleportType::ResetPhysics);
}

void AChaosVehiclePawn::ToggleCamera()
{
    ActiveCameraIndex = (ActiveCameraIndex + 1) % 2;
    UpdateCameraView();
}

void AChaosVehiclePawn::UpdateCameraView()
{
    if (ActiveCameraIndex == 0)
    {
        ChaseCamera->SetActive(true);
        HoodCamera->SetActive(false);
        SpringArm->SetActive(true);
    }
    else
    {
        ChaseCamera->SetActive(false);
        HoodCamera->SetActive(true);
        SpringArm->SetActive(false);
    }
}

float AChaosVehiclePawn::GetSpeedKmh() const
{
    return GetVelocity().Size() * 0.036f;
}

float AChaosVehiclePawn::GetEngineRPM() const
{
    auto* WheeledComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
    return WheeledComp ? WheeledComp->GetEngineRotationSpeed() : 0.0f;
}

int32 AChaosVehiclePawn::GetCurrentGear() const
{
    auto* MoveComp = GetVehicleMovementComponent();
    return MoveComp ? MoveComp->GetCurrentGear() : 0;
}

float AChaosVehiclePawn::CalculateDriftAngle() const
{
    FVector Velocity = GetVelocity();
    FVector Forward = GetActorForwardVector();
    Forward.Z = 0.0f;
    Forward.Normalize();
    Velocity.Z = 0.0f;

    float ForwardSpeed = FVector::DotProduct(Velocity, Forward);
    float TotalSpeed = Velocity.Size();

    if (TotalSpeed < 1.0f)
        return 0.0f;

    float CosAngle = ForwardSpeed / TotalSpeed;
    float AngleRad = FMath::Acos(FMath::Clamp(CosAngle, -1.0f, 1.0f));
    float AngleDeg = FMath::RadiansToDegrees(AngleRad);

    // Determine direction (left or right drift)
    FVector Right = GetActorRightVector();
    Right.Z = 0.0f;
    Right.Normalize();
    float LateralSpeed = FVector::DotProduct(Velocity, Right);

    return FMath::Sign(LateralSpeed) * AngleDeg;
}

float AChaosVehiclePawn::CalculateWheelSlipRatio() const
{
    auto* WheeledComp = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
    if (!WheeledComp)
        return 0.0f;

    float MaxSlip = 0.0f;
    float LinearSpeed = FMath::Max(GetSpeedKmh() / 3.6f, 0.0f); // m/s
    for (UChaosVehicleWheel* Wheel : WheeledComp->Wheels)
    {
        if (!Wheel)
        {
            continue;
        }
        float WheelRadius = Wheel->WheelRadius;
        float AngularSpeed = FMath::Abs(Wheel->GetWheelAngularVelocity());
        float TheoreticalSpeed = AngularSpeed * WheelRadius;

        if (TheoreticalSpeed > 1.0f)
        {
            float Slip = FMath::Abs(TheoreticalSpeed - LinearSpeed) / TheoreticalSpeed;
            MaxSlip = FMath::Max(MaxSlip, Slip);
        }
    }
    return MaxSlip;
}

void AChaosVehiclePawn::OnVehicleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, FVector NormalImpulse,
                                      const FHitResult& Hit)
{
    if (!OtherActor || OtherActor == this)
        return;

    float ImpactSpeed = FMath::Abs(FVector::DotProduct(GetVelocity(), Hit.ImpactNormal));
    float ImpactKmh = ImpactSpeed * 0.036f;

    if (ImpactKmh > 5.0f)
    {
        ACruiseSprintGameMode* GM = Cast<ACruiseSprintGameMode>(GetWorld()->GetAuthGameMode());
        if (GM)
        {
            GM->OnVehicleCollision(ImpactKmh);
        }
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Vehicle collision at %.1f km/h with %s"),
            ImpactKmh, *OtherActor->GetName());
    }
}
