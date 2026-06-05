#include "VehicleAudioComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "ChaosVehiclePawn.h"

UVehicleAudioComponent::UVehicleAudioComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UVehicleAudioComponent::BeginPlay()
{
    Super::BeginPlay();

    if (EngineRevSound)
    {
        EngineAudio = NewObject<UAudioComponent>(GetOwner());
        EngineAudio->RegisterComponent();
        EngineAudio->SetSound(EngineRevSound);
        EngineAudio->bAutoActivate = true;
        EngineAudio->bAllowSpatialization = true;
        EngineAudio->Play();
    }

    if (TireScreechSound)
    {
        ScreechAudio = NewObject<UAudioComponent>(GetOwner());
        ScreechAudio->RegisterComponent();
        ScreechAudio->SetSound(TireScreechSound);
        ScreechAudio->bAutoActivate = false;
        ScreechAudio->bAllowSpatialization = true;
    }

    if (BrakeSquealSound)
    {
        BrakeAudio = NewObject<UAudioComponent>(GetOwner());
        BrakeAudio->RegisterComponent();
        BrakeAudio->SetSound(BrakeSquealSound);
        BrakeAudio->bAutoActivate = false;
        BrakeAudio->bAllowSpatialization = true;
    }
}

void UVehicleAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(GetOwner());
    if (!Vehicle)
        return;

    float SpeedKmh = Vehicle->GetSpeedKmh();
    float RPM = Vehicle->GetEngineRPM();
    float Slip = FMath::Abs(SpeedKmh - LastSpeedKmh) / FMath::Max(1.0f, DeltaTime);

    UpdateEngineSound(SpeedKmh, RPM);
    UpdateScreechSound(SpeedKmh, Slip);
    // Brake input not directly accessible here; would need to expose from pawn

    LastSpeedKmh = SpeedKmh;
}

void UVehicleAudioComponent::UpdateEngineSound(float SpeedKmh, float RPM)
{
    if (!EngineAudio || !EngineRevSound)
        return;

    float RPMPct = FMath::Clamp(RPM / 7000.0f, 0.0f, 1.0f);
    float Pitch = FMath::Lerp(EnginePitchMin, EnginePitchMax, RPMPct);
    float Volume = FMath::Lerp(0.3f, 1.0f, RPMPct) * CurrentVolume;

    EngineAudio->SetPitchMultiplier(Pitch);
    EngineAudio->SetVolumeMultiplier(Volume);

    if (!EngineAudio->IsPlaying())
    {
        EngineAudio->Play();
    }
}

void UVehicleAudioComponent::UpdateScreechSound(float SpeedKmh, float Slip)
{
    if (!ScreechAudio || !TireScreechSound)
        return;

    bool bShouldScreech = SpeedKmh > ScreechThresholdKmh && Slip > ScreechMinSlip;

    if (bShouldScreech && !ScreechAudio->IsPlaying())
    {
        ScreechAudio->SetVolumeMultiplier(0.5f * CurrentVolume);
        ScreechAudio->Play();
    }
    else if (!bShouldScreech && ScreechAudio->IsPlaying())
    {
        ScreechAudio->Stop();
    }
}

void UVehicleAudioComponent::UpdateBrakeSound(float BrakeInput, float SpeedKmh)
{
    if (!BrakeAudio || !BrakeSquealSound)
        return;

    bool bShouldSqueal = BrakeInput > 0.5f && SpeedKmh > 20.0f;

    if (bShouldSqueal && !BrakeAudio->IsPlaying())
    {
        BrakeAudio->SetVolumeMultiplier(BrakeInput * CurrentVolume);
        BrakeAudio->Play();
    }
    else if (!bShouldSqueal && BrakeAudio->IsPlaying())
    {
        BrakeAudio->Stop();
    }
}

void UVehicleAudioComponent::OnCollision(float ImpactSpeedKmh)
{
    if (!CollisionSound)
        return;

    float Volume = FMath::Clamp(ImpactSpeedKmh / 100.0f, 0.1f, 1.0f) * CurrentVolume;
    UGameplayStatics::PlaySoundAtLocation(GetWorld(), CollisionSound, GetOwner()->GetActorLocation(), Volume);
}

void UVehicleAudioComponent::SetMasterVolume(float Volume)
{
    CurrentVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
}
