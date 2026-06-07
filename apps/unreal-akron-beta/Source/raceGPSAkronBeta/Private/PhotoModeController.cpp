// Copyright raceGPS. All Rights Reserved.

#include "PhotoModeController.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HighResScreenshot.h"

APhotoModeController::APhotoModeController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;

    FreeCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FreeCamera"));
    FreeCamera->bUsePawnControlRotation = false;
    RootComponent = FreeCamera;

    InitializeFilters();
}

void APhotoModeController::BeginPlay()
{
    Super::BeginPlay();
    FreeCamera->SetActive(false);
}

void APhotoModeController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bPhotoModeActive && FreeCamera)
    {
        // Smooth camera movement could be added here
    }
}

void APhotoModeController::EnterPhotoMode()
{
    if (bPhotoModeActive)
        return;

    bPhotoModeActive = true;

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        CachedPlayerLocation = PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector::ZeroVector;
        CachedPlayerRotation = PC->GetControlRotation();
        PC->SetViewTarget(this);
    }

    FreeCamera->SetActive(true);
    SetActorLocation(CachedPlayerLocation);
    SetActorRotation(CachedPlayerRotation);

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Photo mode entered"));
}

void APhotoModeController::ExitPhotoMode()
{
    if (!bPhotoModeActive)
        return;

    bPhotoModeActive = false;

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC && PC->GetPawn())
    {
        PC->SetViewTarget(PC->GetPawn());
    }

    FreeCamera->SetActive(false);
    ResetPostProcess();

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Photo mode exited"));
}

void APhotoModeController::SetCameraPosition(const FVector& Pos, const FRotator& Rot)
{
    SetActorLocation(Pos);
    SetActorRotation(Rot);
}

void APhotoModeController::ApplyFilter(const FString& FilterName)
{
    CurrentFilter = FilterName;
    if (!FreeCamera)
        return;

    FLinearColor* FoundColor = FilterPresets.Find(FilterName);
    if (FoundColor)
    {
        FPostProcessSettings Settings;
        Settings.bOverride_ColorGamma = true;
        Settings.ColorGamma = FVector4(FoundColor->R, FoundColor->G, FoundColor->B, 1.0f);
        FreeCamera->PostProcessSettings = Settings;
        UE_LOG(LogTemp, Log, TEXT("[raceGPS] Photo filter applied: %s"), *FilterName);
    }
    else
    {
        ResetPostProcess();
    }
}

void APhotoModeController::CaptureScreenshot()
{
    FString ScreenshotDir = FPaths::ProjectSavedDir() / TEXT("Screenshots");
    FPaths::MakeStandardFilename(ScreenshotDir);
    IFileManager::Get().MakeDirectory(*ScreenshotDir, true);

    FString Filename = FString::Printf(TEXT("raceGPS_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
    FString FullPath = ScreenshotDir / Filename;

    FScreenshotRequest::RequestScreenshot(FullPath, false, false);

    UE_LOG(LogTemp, Log, TEXT("[raceGPS] Screenshot captured: %s"), *FullPath);
}

FVector APhotoModeController::GetCameraLocation() const
{
    return FreeCamera ? FreeCamera->GetComponentLocation() : FVector::ZeroVector;
}

FRotator APhotoModeController::GetCameraRotation() const
{
    return FreeCamera ? FreeCamera->GetComponentRotation() : FRotator::ZeroRotator;
}

void APhotoModeController::InitializeFilters()
{
    FilterPresets.Add(TEXT("None"), FLinearColor(1.0f, 1.0f, 1.0f));
    FilterPresets.Add(TEXT("Vintage"), FLinearColor(1.2f, 1.0f, 0.8f));
    FilterPresets.Add(TEXT("Noir"), FLinearColor(0.5f, 0.5f, 0.5f));
    FilterPresets.Add(TEXT("Cyber"), FLinearColor(0.8f, 1.0f, 1.2f));
    FilterPresets.Add(TEXT("Sunset"), FLinearColor(1.2f, 0.9f, 0.7f));
}

void APhotoModeController::ResetPostProcess()
{
    if (FreeCamera)
    {
        FreeCamera->PostProcessSettings = FPostProcessSettings();
    }
}
