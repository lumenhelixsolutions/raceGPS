#include "ChaosVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/ArrowComponent.h"
#include "VehicleTuningData.h"
#include "VehicleAudioComponent.h"
#include "CruiseSprintGameMode.h"

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
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> VehicleMesh(TEXT("/Game/Vehicles/Sedan/Sedan_SkelMesh.Sedan_SkelMesh"));
    if (VehicleMesh.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(VehicleMesh.Object);
    }
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

    // Basic engine torque curve
    MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
    MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.0f, 400.0f);
    MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(2000.0f, 500.0f);
    MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(4000.0f, 450.0f);
    MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(6000.0f, 300.0f);
}

void AChaosVehiclePawn::ApplyTuningData()
{
    auto* MoveComp = GetVehicleMovementComponent();
    if (!MoveComp || !TuningData)
        return;

    // Vehicle mass and aero
    MoveComp->Mass = TuningData->VehicleMass;
    MoveComp->DragCoefficient = TuningData->DragCoefficient;
    MoveComp->ChassisWidth = TuningData->ChassisWidth;
    MoveComp->ChassisHeight = TuningData->ChassisHeight;
    MoveComp->DownforceCoefficient = TuningData->DownForceCoefficient;
    MoveComp->DownforceOffset = TuningData->DownForceOffset;

    // Engine
    MoveComp->MaxEngineRPM = TuningData->MaxEngineRPM;
    MoveComp->EngineSetup.MaxRPM = TuningData->MaxEngineRPM;
    MoveComp->EngineSetup.IdleRPM = TuningData->IdleRPM;
    MoveComp->EngineSetup.BrakeEffect = TuningData->BrakeTorque;

    // Brake torque
    MoveComp->BrakeTorque = TuningData->BrakeTorque;
    MoveComp->HandbrakeTorque = TuningData->HandbrakeTorque;

    // Steering
    MoveComp->SteeringSetup.SteeringCurve = TuningData->SteeringCurve;
    MoveComp->SteeringSetup.AckermannAccuracy = TuningData->AckermannAccuracy;

    // Transmission
    MoveComp->TransmissionSetup.FinalRatio = TuningData->Transmission.FinalDriveRatio;
    MoveComp->TransmissionSetup.ReverseGearRatio = TuningData->Transmission.ReverseGearRatio;
    MoveComp->TransmissionSetup.UpShiftRPM = TuningData->Transmission.UpShiftRPM;
    MoveComp->TransmissionSetup.DownShiftRPM = TuningData->Transmission.DownShiftRPM;
    MoveComp->TransmissionSetup.ChangeUpTime = TuningData->Transmission.ChangeUpTime;
    MoveComp->TransmissionSetup.ChangeDownTime = TuningData->Transmission.ChangeDownTime;
    MoveComp->TransmissionSetup.GearRatios.Empty();
    for (float Ratio : TuningData->Transmission.GearRatios)
    {
        FVehicleGearData Gear;
        Gear.Ratio = Ratio;
        Gear.DownRatio = TuningData->Transmission.DownShiftRPM / TuningData->MaxEngineRPM;
        Gear.UpRatio = TuningData->Transmission.UpShiftRPM / TuningData->MaxEngineRPM;
        MoveComp->TransmissionSetup.GearRatios.Add(Gear);
    }

    // Differential
    MoveComp->DifferentialSetup.DifferentialType = TuningData->Differential.DifferentialType;
    MoveComp->DifferentialSetup.FrontRearSplit = TuningData->Differential.FrontRearSplit;
    MoveComp->DifferentialSetup.FrontLeftRightSplit = TuningData->Differential.FrontLeftRightSplit;
    MoveComp->DifferentialSetup.RearLeftRightSplit = TuningData->Differential.RearLeftRightSplit;
    MoveComp->DifferentialSetup.CentreBias = TuningData->Differential.CentreBias;
    MoveComp->DifferentialSetup.FrontBias = TuningData->Differential.FrontBias;
    MoveComp->DifferentialSetup.RearBias = TuningData->Differential.RearBias;

    // Wheels
    for (int32 i = 0; i < TuningData->Wheels.Num() && i < MoveComp->WheelSetups.Num(); ++i)
    {
        SetupWheel(i, TuningData->Wheels[i]);
    }
}

void AChaosVehiclePawn::SetupWheel(int32 WheelIndex, const FWheelTuning& Wheel)
{
    auto* MoveComp = GetVehicleMovementComponent();
    if (!MoveComp || WheelIndex >= MoveComp->WheelSetups.Num())
        return;

    FChaosWheelSetup& Setup = MoveComp->WheelSetups[WheelIndex];
    Setup.WheelRadius = Wheel.Radius;
    Setup.WheelWidth = Wheel.Width;
    Setup.WheelMass = Wheel.Mass;
    Setup.bSteerable = Wheel.SteerAngle != 0.0f;
    Setup.bDriven = Wheel.bDrive;
    Setup.bHandbrake = Wheel.bHandbrake;

    // Suspension
    if (Setup.Suspension)
    {
        Setup.Suspension->SpringRate = Wheel.SuspensionStiffness;
        Setup.Suspension->DampingRatio = Wheel.SuspensionDamping;
        Setup.Suspension->MaxRaise = Wheel.MaxRaise;
        Setup.Suspension->MaxDrop = Wheel.MaxDrop;
        Setup.Suspension->SuspensionForceOffset = Wheel.SuspensionForceOffset;
    }
}

void AChaosVehiclePawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    auto* MoveComp = GetVehicleMovementComponent();
    if (MoveComp)
    {
        MoveComp->SetThrottleInput(CurrentThrottle * ThrottleSensitivity);
        MoveComp->SetSteeringInput(CurrentSteering * SteeringSensitivity);
        MoveComp->SetBrakeInput(CurrentBrake);
        MoveComp->SetHandbrakeInput(bHandbrake);
    }
}

void AChaosVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis(TEXT("Throttle"), this, &AChaosVehiclePawn::SetThrottleInput);
    PlayerInputComponent->BindAxis(TEXT("Steer"), this, &AChaosVehiclePawn::SetSteeringInput);
    PlayerInputComponent->BindAxis(TEXT("Brake"), this, &AChaosVehiclePawn::SetBrakeInput);
    PlayerInputComponent->BindAction(TEXT("Handbrake"), IE_Pressed, this, &AChaosVehiclePawn::SetHandbrakeInput, true);
    PlayerInputComponent->BindAction(TEXT("Handbrake"), IE_Released, this, &AChaosVehiclePawn::SetHandbrakeInput, false);
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
    auto* MoveComp = GetVehicleMovementComponent();
    return MoveComp ? MoveComp->GetEngineRotationSpeed() : 0.0f;
}

int32 AChaosVehiclePawn::GetCurrentGear() const
{
    auto* MoveComp = GetVehicleMovementComponent();
    return MoveComp ? MoveComp->GetCurrentGear() : 0;
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
