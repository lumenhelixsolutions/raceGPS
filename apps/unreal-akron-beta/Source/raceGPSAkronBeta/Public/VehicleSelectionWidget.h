#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VehicleTuningData.h"
#include "VehicleSelectionWidget.generated.h"

UCLASS()
class RACEGPSAKRONBETA_API UVehicleSelectionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void PopulateVehicleList(const TArray<UVehicleTuningData*>& Vehicles);

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    UVehicleTuningData* GetSelectedVehicle() const;

    UFUNCTION(BlueprintCallable, Category = "raceGPS|Vehicle")
    void OnVehicleSelected(const FString& SelectedItem, ESelectInfo::Type SelectionType);

    UPROPERTY(meta = (BindWidget))
    class UComboBoxString* VehicleSelector;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* VehicleNameText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* VehicleDescriptionText;

    UPROPERTY(meta = (BindWidget))
    class UTextBlock* VehicleStatsText;

    UPROPERTY(BlueprintReadOnly, Category = "raceGPS|Vehicle")
    TArray<UVehicleTuningData*> AvailableVehicles;

protected:
    UFUNCTION()
    void UpdateVehicleInfo();
};
