#include "CompassWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "ChaosVehiclePawn.h"

void UCompassWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UCompassWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    AChaosVehiclePawn* Vehicle = Cast<AChaosVehiclePawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
    if (!Vehicle)
        return;

    float Yaw = Vehicle->GetActorRotation().Yaw;
    UpdateCompass(Yaw);
}

void UCompassWidget::UpdateCompass(float YawDegrees)
{
    // Normalize to 0-360
    YawDegrees = FMath::Fmod(YawDegrees + 360.0f, 360.0f);

    if (HeadingText)
    {
        FString Cardinal = GetCardinalDirection(YawDegrees);
        HeadingText->SetText(FText::FromString(FString::Printf(TEXT("%s %.0f°"), *Cardinal, YawDegrees)));
    }

    if (CompassStrip)
    {
        // Shift the compass strip UV based on yaw
        float UOffset = YawDegrees / 360.0f;
        // In a real implementation, this would pan a texture
        // For now, we just track the value
        (void)UOffset;
    }
}

FString UCompassWidget::GetCardinalDirection(float YawDegrees) const
{
    if (YawDegrees >= 337.5f || YawDegrees < 22.5f)
        return TEXT("N");
    if (YawDegrees >= 22.5f && YawDegrees < 67.5f)
        return TEXT("NE");
    if (YawDegrees >= 67.5f && YawDegrees < 112.5f)
        return TEXT("E");
    if (YawDegrees >= 112.5f && YawDegrees < 157.5f)
        return TEXT("SE");
    if (YawDegrees >= 157.5f && YawDegrees < 202.5f)
        return TEXT("S");
    if (YawDegrees >= 202.5f && YawDegrees < 247.5f)
        return TEXT("SW");
    if (YawDegrees >= 247.5f && YawDegrees < 292.5f)
        return TEXT("W");
    return TEXT("NW");
}
