#include "ChaosVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/ArrowComponent.h"

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

    // Default vehicle mesh placeholder
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> VehicleMesh(TEXT("/Game/Vehicles/Sedan/Sedan_SkelMesh.Sedan_SkelMesh"));
    if (VehicleMesh.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(VehicleMesh.Object);
    }

    // Vehicle movement defaults
    auto* MoveComp = GetVehicleMovementComponent();
    if (MoveComp)
    {
        MoveComp->Mass = 1500.0f;
        MoveComp->DragCoefficient = 0.3f;
        MoveComp->ChassisWidth = 180.0f;
        MoveComp->ChassisHeight = 140.0f;
        MoveComp->InertiaTensorScale = FVector(1.0f, 1.0f, 1.0f);
        MoveComp->MaxEngineRPM = 6000.0f;
        MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->Reset();
        MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(0.0f, 400.0f);
        MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(2000.0f, 500.0f);
        MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(4000.0f, 450.0f);
        MoveComp->EngineSetup.TorqueCurve.GetRichCurve()->AddKey(6000.0f, 300.0f);
    }
}

void AChaosVehiclePawn::BeginPlay()
{
    Super::BeginPlay();
    InitChaosVehicleMovement();
    UpdateCameraView();
}

void AChaosVehiclePawn::InitChaosVehicleMovement()
{
    auto* MoveComp = GetVehicleMovementComponent();
    if (!MoveComp) return;

    // Ensure we have a proper physics setup for Chaos Vehicles
    MoveComp->StopMovementImmediately();
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
