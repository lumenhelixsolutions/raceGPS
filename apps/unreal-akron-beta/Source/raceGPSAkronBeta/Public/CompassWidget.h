#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CompassWidget.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API UCompassWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* HeadingText;

    UPROPERTY(meta = (BindWidget))
    class UImage* CompassStrip;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    float StripWidth = 400.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "raceGPS|HUD")
    int32 DegreeMarkers = 36; // Every 10 degrees

protected:
    void UpdateCompass(float YawDegrees);
    FString GetCardinalDirection(float YawDegrees) const;
};
